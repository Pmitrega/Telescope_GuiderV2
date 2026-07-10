import json
import math
import paho.mqtt.client as mqtt
import time
from enum import Enum
from pathlib import Path
from datetime import datetime
import csv
import numpy as np
import identification

STAR_LOC_TOPIC = "guider/detected_stars"
TRACKED_STAR_TOPIC = "guider/tracked_star"
SET_POSITION_TOPIC = "guider/set_position"
SET_POSITION_REQ_TOPIC = "guider/set_position_req"
TRACKING_ERROR_TOPIC = "guider/tracking_error"
TRACKED_STAR_REQ_TOPIC = "guider/tracked_star_req"

GUIDER_MODE_REQ_TOPIC = "guider/mode_req"

MQTT_BROKER = "localhost"  # Change if remote
MQTT_PORT = 1883


class requestList:
    def __init__(self, runCalibration = False, ):
        self.runCalibration = runCalibration


class Star:
    def __init__(self, x_pos:float, y_pos:float):
        self.x_pos = x_pos
        self.y_pos = y_pos
    
    def __repr__(self):
        return f"Star(x={self.x_pos:.2f}, y={self.y_pos:.2f})"
    
    def distance_to(self, other_star):
        """Calculate Cartesian distance to another star."""
        dx = self.x_pos - other_star.x_pos
        dy = self.y_pos - other_star.y_pos
        return math.sqrt(dx**2 + dy**2)

class StarLogger:
    def __init__(self, log_dir="logs"):
        self.log_dir = Path(log_dir)
        self.log_dir.mkdir(parents=True, exist_ok=True)

        self.file = None
        self.writer = None

        self._create_new_file()

    def _create_new_file(self):
        filename = datetime.now().strftime("%Y-%m-%d_%H-%M-%S.csv")

        self.file = open(filename, "w", newline="")
        self.writer = csv.writer(self.file)

        self.writer.writerow([
            "date",
            "time",
            "x_pos",
            "y_pos",
            "ra_speed",
            "dec_speed"
        ])

        print(f"Logging to {filename}")

    def log(self, star, ra_speed, dec_speed):
        now = datetime.now()

        self.writer.writerow([
            now.strftime("%Y-%m-%d"),
            now.strftime("%H:%M:%S"),
            star.x_pos,
            star.y_pos,
            ra_speed,
            dec_speed
        ])

        self.file.flush()

    def close(self):
        if self.file:
            self.file.close()


class StarList:
    def __init__(self):
        self.stars = None
        ## previous position of tracked star
        self.tracked_star_prev = None
        ## current position of tracked star
        self.tracked_star = None
        
        self.max_star_search_retry = 3
        self.search_retry = 0
    
    def __repr__(self):
        if self.stars is None:
            return "StarList(stars=None)"
        stars_repr = ", ".join(repr(star[0]) for star in self.stars)
        return f"StarList([{stars_repr}], tracked={self.tracked_star})"

    
    def updateStarList(self, json_str):
        data = json.loads(json_str)
        self.stars = [ [Star(x, y), area] for x, y, area in data["stars"]]

    def __get_best_to_track(self):
        if self.stars is None or len(self.stars) == 0:
            return None
        # Return star with highest area (3rd value)
        return max(self.stars, key=lambda star_item: star_item[1])[0]
    

    def __find_matching_star(self, target_star, max_distance=50):
        """Find the closest star to target_star within max_distance, or None if none found."""
        if self.stars is None or len(self.stars) == 0:
            return None
        
        closest_star = None
        min_distance = float('inf')
        
        for star_item in self.stars:
            star = star_item[0]
            distance = star.distance_to(target_star)
            if distance <= max_distance and distance < min_distance:
                min_distance = distance
                closest_star = star
        
        return closest_star
    
    def __handle_failed_search(self):
        self.search_retry = self.search_retry + 1
        if self.search_retry > self.max_star_search_retry:
            print("Lost a star, reached max search retries")
            self.tracked_star = None
            self.tracked_star_prev = None
        else:
            print(f"Failed to find a star, retrying {self.search_retry}/{self.max_star_search_retry}")

    
    def updateTrackedStar(self):
        if self.tracked_star is None:
            self.tracked_star = self.__get_best_to_track()
            self.tracked_star_prev = None
        else:
            self.tracked_star_prev = self.tracked_star
            self.tracked_star = self.__find_matching_star(self.tracked_star)
            if (self.tracked_star is None):
                self.__handle_failed_search()
            else:
                self.search_retry = 0
        return self.tracked_star
    
    def getTrackedStar(self):
        return self.tracked_star

    def setNearestToBeTracked(self, x_pos: float, y_pos: float, max_distance: float = 50):
        """Find the nearest star to the given position and set it as tracked."""
        target_star = Star(x_pos, y_pos)
        nearest_star = self.__find_matching_star(target_star, max_distance)
        if nearest_star is not None:
            self.tracked_star = nearest_star
            self.tracked_star_prev = None
            self.search_retry = 0
            print(f"Set tracked star to nearest: {nearest_star}")
        else:
            print(f"No star found within {max_distance} pixels of ({x_pos}, {y_pos})")


class ControllerMode(Enum):
    MANUAL = 1
    AUTO = 2
    GOTO = 3
    IDENTIFICATION = 4

class Controller:
    def __init__(self, mqtt_client: mqtt.Client):
        self.mqtt_client = mqtt_client
        self.star_list = StarList()
        self.set_point = None
        self.ra_pos = 129.226379
        self.dec_pos = 27.406087
        self.ra_speed = 0
        self.dec_speed = 0
        self.__earth_rot_speed = 15/3600
        self.mode = ControllerMode.AUTO
        self.first_run = True
        self.logger = StarLogger()
        self.ra_target = 130.07466
        self.dec_target = 28
        self.goto_speed = 300



        self.identifier = None
        self.identification_start_ts = None
        self.identification_ctn_off = 0
        self.identification_ctn_on = 0
        self.identification_ctn_backlash_removal = 0
        ### In format x, y, timestamp in seconds.
        self.ident_buffer_off = []
        self.ident_buffer_on  = []

        data = None

        with open("config.json") as f:
            data = json.load(f)

        self.mode = ControllerMode[data["mode"]]
        self.off_pixel_speed = np.array(data["off_pixel_speed"]).reshape(-1, 1)
        self.on_pixel_speed = np.array(data["on_pixel_speed"]).reshape(-1, 1)
        self.sec_per_pix = data["sec_per_pix"]
        self.set_point = data["set_point"]

        print("Last identification results:")
        print(f"mode: {self.mode.name}")
        print(f"sec per pixel: {self.sec_per_pix}")

        print(f"off pixel speed: {self.off_pixel_speed}")
        print(f"on pixel speed: {self.on_pixel_speed}")
        if (self.off_pixel_speed[0] !=0):
            self.identifier = identification.Indentifer(self.off_pixel_speed * self.sec_per_pix, self.on_pixel_speed * self.sec_per_pix)
            print(self.identifier.getTelCtrlMatrix(4.1))

        mot_ra_speed_vect = None
        mot_dec_speed_vect = None

        self.dead_zone = 0.25

    def __update_sky_pos(self, time_diff_s):
        self.ra_pos = (self.ra_pos + (self.ra_speed/3600 + self.__earth_rot_speed) * time_diff_s) % 360
        self.dec_pos = self.dec_pos + self.dec_speed/3600 * time_diff_s

    def set_sky_position(self, ra_pos, dec_pos):
        self.ra_pos = ra_pos
        self.dec_pos = dec_pos

    def _get_move_sign(self, tar, cur):
        diff = tar - cur
        if diff < 0:   

            
            return -1
        elif diff > 0:
            return 1
        else:
            return 0
        
    def average_speed_fit(self, points):
        """
        points: Nx3 array [x_pos, y_pos, timestamp]

        Returns:
            vx, vy
        """
        points = np.asarray(points)

        x = points[:, 0]
        y = points[:, 1]
        t = points[:, 2]

        # Shift time to start at zero for numerical stability
        t_sec = t - t[0]

        vx, _ = np.polyfit(t_sec, x, 1)
        vy, _ = np.polyfit(t_sec, y, 1)

        return vx, vy
    
    def _gotoHandler(self):
        ra_dir = self._get_move_sign(self.ra_target, self.ra_pos) * (abs(self.ra_target - self.ra_pos) > self.dead_zone)
        dec_dir = self._get_move_sign(self.dec_target, self.dec_pos) * (abs(self.dec_target - self.dec_pos) > self.dead_zone)
        self.setSpeeds(ra_dir * self.goto_speed, dec_dir * self.goto_speed)

    def _identificationHandler(self):
            tracked_star = self.star_list.getTrackedStar()
            # Shutdown motor, get start timestamp
            if(self.identification_ctn_off == 0):
                self.identification_start_ts = time.time()
                print("Starting identification ...")
            # wait for 20 iteration with motors turned off
            if(self.identification_ctn_off  < 20):
                print(f"Motors off ...{self.identification_ctn_off}")
                self.setSpeeds(0, 0)
                self.identification_ctn_off = self.identification_ctn_off + 1
                self.ident_buffer_off.append([tracked_star.x_pos, tracked_star.y_pos, time.time() - self.identification_start_ts])

            # To eliminate backlash switch on motor
            elif(self.identification_ctn_backlash_removal < 5):
                self.setSpeeds(-20, 0)
                self.identification_ctn_backlash_removal = self.identification_ctn_backlash_removal + 1
            # Now set speed to be equal earth rotation
            elif(self.identification_ctn_on < 20):
                print(f"Motors on ...{self.identification_ctn_on}")
                self.setSpeeds(-15, 0)
                self.identification_ctn_on = self.identification_ctn_on + 1
                self.ident_buffer_on.append([tracked_star.x_pos, tracked_star.y_pos, time.time() - self.identification_start_ts])
            # when all steps has been completed fit point to linear function to get pixel/s when motor is off and on
            else:
                vx, vy = self.average_speed_fit(self.ident_buffer_off)
                self.off_pixel_speed = [[vx], [vy]]
                arr_on = np.array([vx, vy])

                vx, vy = self.average_speed_fit(self.ident_buffer_on)

                self.on_pixel_speed = [vx, vy]
                arr_off = np.array([[vx], [vy]])
                # Save updated values
                with open("config.json", "r") as f:
                    data = json.load(f)

                data["off_pixel_speed"] = self.off_pixel_speed
                data["on_pixel_speed"] = self.on_pixel_speed
                data["mode"] = "AUTO"

                with open("config.json", "w") as f:
                    json.dump(data, f, indent=4)

                print("---------------motor off positions --------------")
                for i in range(len(self.ident_buffer_off)):
                    print(self.ident_buffer_off[i])
                print("---------------motor on positions --------------")
                for i in range(len(self.ident_buffer_on)):
                    print(self.ident_buffer_on[i])

                print("Identified speeds:")
                print(f"V off: vx = {self.off_pixel_speed[0]}, vy = {self.off_pixel_speed[1]}")
                print(f"V on: vx = {self.on_pixel_speed[0]}, vy = {self.on_pixel_speed[1]}")

                self.identifier = identification.Indentifer(arr_off * self.sec_per_pix, arr_on * self.sec_per_pix)

                print("Open Loop control: Ra/Dec", self.identifier.getOpenLoopCtr())

                """Clear identification buffers"""
                self.identification_ctn_off = 0
                self.identification_ctn_on = 0
                self.ident_buffer_off = []
                self.ident_buffer_on = []
                

                """ If identification is complete shange mode to auto"""
                self.setSetPoint(tracked_star.x_pos, tracked_star.y_pos)
                self.mode = ControllerMode.AUTO

    def controllerUpdate(self, star_list: str):
        if self.first_run:
            self.setSpeeds(-15, 0)
            self.first_run = False
        self.star_list.updateStarList(star_list)
        self.star_list.updateTrackedStar()
        self.__update_sky_pos(0.5)
        # print(self.getSetPoint())
        # print(self.ra_pos, " ", self.dec_pos)
        tracked_star = self.star_list.getTrackedStar()
        if tracked_star is not None:
            publishTrackedStar(tracked_star, client)
            self.logger.log(tracked_star, self.ra_speed, self.dec_speed)
            print(tracked_star)

    
        if self.mode == ControllerMode.MANUAL:
            #Do nothing
            pass
        elif self.mode == ControllerMode.GOTO:
            self._gotoHandler()  
        elif tracked_star is not None and self.mode == ControllerMode.IDENTIFICATION:
            self._identificationHandler()
        elif self.mode == ControllerMode.AUTO:
            if(self.set_point != 0) and tracked_star is not None:
                self.error =  np.array([tracked_star.x_pos, tracked_star.y_pos]) - np.array(self.set_point)
                self.error = self.error.reshape(-1, 1)
                self.ra_dec_ctrl = self.identifier.getTelCtrlMatrix(self.sec_per_pix) @ self.error
                print("ra_dec_ctrl")
                print(self.ra_dec_ctrl)
                self.gain = 0.1
                ra_ctrl = self.ra_dec_ctrl[0][0] * self.gain
                dec_ctrl = self.ra_dec_ctrl[1][0] * self.gain
                
                print(self.error)
                print("ra_ctrl")
                print(ra_ctrl)
                print("dec_ctrl")
                print(dec_ctrl)

                if (ra_ctrl > 30):
                    ra_ctrl = 30
                elif (ra_ctrl < -30):
                    ra_ctrl = -30
                
                if (dec_ctrl > 20):
                    dec_ctrl = 20
                elif (dec_ctrl < -20):
                    dec_ctrl = -20

                self.setSpeeds(ra_ctrl, dec_ctrl)
            
            # ra_dir = self._get_move_sign(self.ra_target, self.ra_pos) * (abs(self.ra_target - self.ra_pos) > self.dead_zone)
            # dec_dir = self._get_move_sign(self.dec_target, self.dec_pos) * (abs(self.dec_target - self.dec_pos) > self.dead_zone)
            # distance_ra = self.ra_target - self.ra_pos
            # distance_dec = self.dec_target - self.dec_speed
            # self.setSpeeds(ra_dir * self.goto_speed, dec_dir * self.goto_speed)

        

    def getSkyPosRaDec(self):
        return [self.ra_pos, self.dec_pos]

    def controllerFSM(star_list: StarList, requests: requestList):
        pass
    
    def setSetPoint(self, x_set, y_set):
        self.set_point = [x_set, y_set]
        return self.set_point

    def getSetPoint(self):
        return self.set_point

    def setSpeeds(self, ra_speed: float, dec_speed: float):
        if self.mqtt_client.is_connected():
            message = f"{{\"ra_speed\": {ra_speed}, \"dec_speed\": {dec_speed}}}"
            print(message)
            self.mqtt_client.publish("guider/motor_speed", message)
            self.ra_speed = ra_speed
            self.dec_speed = dec_speed
        else:
            print("ERROR: mqtt_client not connected")



client = mqtt.Client()
controller = Controller(client)


def publishTrackedStar(star: Star, client: mqtt.Client):
    if client.is_connected():
        message = json.dumps({"tracked_star": [star.x_pos, star.y_pos]})
        client.publish(TRACKED_STAR_TOPIC, message)


def publishSetPoint(x_set: float, y_set: float, client: mqtt.Client):
    if client.is_connected():
        message = json.dumps({"set_position": [x_set, y_set]})
        client.publish(SET_POSITION_TOPIC, message)


def publishError(x_err: float, y_err: float, client: mqtt.Client):
    if client.is_connected():
        message = json.dumps({"tracking_error": [x_err, y_err]})
        client.publish(TRACKING_ERROR_TOPIC, message)


# Called when a message is received
def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode("utf-8").strip().lower()
        
        if(msg.topic == STAR_LOC_TOPIC):
            controller.controllerUpdate(msg.payload)
            # controller.setSpeeds(-100, 0)

        elif(msg.topic == SET_POSITION_REQ_TOPIC):
            print(f"Received: {payload} on topic {msg.topic}")
            try:
                data = json.loads(payload)
                req_star = data.get("req_star")
                if req_star and isinstance(req_star, list) and len(req_star) >= 2:
                    pos_x, pos_y = float(req_star[0]), float(req_star[1])
                    sp_pos = controller.setSetPoint(pos_x, pos_y)
                    publishSetPoint(sp_pos[0], sp_pos[1], client)
                else:
                    print("Invalid req_star format: expected array [posx, posy]")
            except json.JSONDecodeError as e:
                print(f"JSON decode error for set position request: {e}")
        elif(msg.topic == TRACKED_STAR_REQ_TOPIC):
            print(f"Received: {payload} on topic {msg.topic}")
            try:
                data = json.loads(payload)
                req_star = data.get("req_star")
                if req_star and isinstance(req_star, list) and len(req_star) >= 2:
                    pos_x, pos_y = req_star[0], req_star[1]
                    controller.star_list.setNearestToBeTracked(float(pos_x), float(pos_y))
                    print("setign star to  be tracked")
                else:
                    print("Invalid req_star format: expected array [posx, posy]")
            except json.JSONDecodeError as e:
                print(f"JSON decode error for tracked star req: {e}")
    except Exception as e:
        print(f"Error processing message: {e}")

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT broker.")
        client.subscribe(STAR_LOC_TOPIC)
        client.subscribe(TRACKED_STAR_REQ_TOPIC)
        client.subscribe(SET_POSITION_REQ_TOPIC)
        print(f"Subscribed to {STAR_LOC_TOPIC}, {TRACKED_STAR_REQ_TOPIC}, and {SET_POSITION_REQ_TOPIC}")
    else:
        print(f"Failed to connect, return code {rc}")


if __name__ == "__main__":
    client.max_queued_messages_set(4)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.loop_forever()

