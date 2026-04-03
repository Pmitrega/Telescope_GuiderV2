import json
import math
import paho.mqtt.client as mqtt
import time
from enum import Enum


STAR_LOC_TOPIC = "guider/detected_stars"
TRACKED_STAR_TOPIC = "guider/tracked_star"
SET_POSITION_TOPIC = "guider/set_position"
SET_POSITION_REQ_TOPIC = "guider/set_position_req"
TRACKING_ERROR_TOPIC = "guider/tracking_error"
TRACKED_STAR_REQ_TOPIC = "guider/tracked_star_req"
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

class Controller:
    def __init__(self, mqtt_client: mqtt.Client):
        self.mqtt_client = mqtt_client
        self.star_list = StarList()
        self.set_point = None
        self.ra_pos = 106.645654
        self.dec_pos = 25.00
        self.ra_speed = 0
        self.dec_speed = 0
        self.__earth_rot_speed = 15/3600
        self.mode = ControllerMode.GOTO
        self.first_run = True

        self.ra_target = 106.900000
        self.dec_target = 22.900000
        self.goto_speed = 100

        self.dead_zone = 0.25

    def __update_sky_pos(self, time_diff_s):
        self.ra_pos = self.ra_pos + (self.ra_speed/3600  + self.__earth_rot_speed) * time_diff_s % 360
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

    def controllerUpdate(self, star_list: str):
        if self.first_run:
            self.setSpeeds(0, 0)
            self.first_run = False
        self.star_list.updateStarList(star_list)
        self.star_list.updateTrackedStar()
        self.__update_sky_pos(0.5)
        print(self.getSetPoint())
        print(self.ra_pos, " ", self.dec_pos)
        tracked_star = self.star_list.getTrackedStar()
        if tracked_star is not None:
            publishTrackedStar(tracked_star, client)

        if self.mode == ControllerMode.GOTO:
            ra_dir = self._get_move_sign(self.ra_target, self.ra_pos) * (abs(self.ra_target - self.ra_pos) > self.dead_zone)
            dec_dir = self._get_move_sign(self.dec_target, self.dec_pos) * (abs(self.dec_target - self.dec_pos) > self.dead_zone)
            distance_ra = self.ra_target - self.ra_pos
            distance_dec = self.dec_target - self.dec_speed

            self.setSpeeds(ra_dir * self.goto_speed, dec_dir * self.goto_speed)

        

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

