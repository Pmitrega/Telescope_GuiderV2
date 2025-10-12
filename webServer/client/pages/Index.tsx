import { useState, useEffect, useRef } from "react";
import { Progress } from "@/components/ui/progress";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { Button } from "@/components/ui/button";
import {Slider} from "@/components/ui/slider"; // adjust path if needed
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Checkbox } from "@/components/ui/checkbox";
import JSZip from "jszip";
import { saveAs } from "file-saver";
import {
  Play,
  Pause,
  Square,
  Settings,
  Search,
  Calendar,
  Power,
  Activity,
  Zap,
  Target,
  Camera,
  Timer,
  BarChart3,
  Monitor,
  Stars,
} from "lucide-react";
import { cn } from "@/lib/utils";
import mqtt from "mqtt";
import { set } from "date-fns";

interface DataPoint {
  x: number;
  y: number;
  type: "target" | "reference" | "variable" | "artifact";
  magnitude: number;
  id: string;
}

export default function Index() {
  const [isCapturing, setIsCapturing] = useState(false);
  const [isAutoMode, setIsAutoMode] = useState(false);
  const [ra_speed_slider, setRaSpeed] = useState([0]);
  const [dec_speed_slider, setDecSpeed] = useState([0]);
  const [gain, setGain] = useState([150]);
  const [captureTime, setCaptureTime] = useState("22:34:45");
  const [targetName, setTargetName] = useState("NGC 4567");
  const [observatoryStatus, setObservatoryStatus] = useState("READY");
  const [batteryLevel, setBatteryLevel] = useState(1351);
  const [voltage, setVoltage] = useState(395);
  const [current, setCurrent] = useState(2.1);
  const [temperature, setTemperature] = useState(-12.4);
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [zoom, setZoom] = useState(1);
  const [offset, setOffset] = useState({ x: 0, y: 0 });
  const minZoomRef = useRef(1); // To keep the minimum zoom scale based on image size
  const [dataPoints, setDataPoints] = useState<DataPoint[]>([]);
  const [enableCompression, setEnableCompression] = useState(true);
  const [batchMode, setBatchMode] = useState(false);
  const [showLowFlux, setShowLowFlux] = useState(true);
  const [showHighFlux, setShowHighFlux] = useState(true);
  const [saveRaw, setSaveRaw] = useState(false);
  const [selectedAlgorithm, setSelectedAlgorithm] = useState("PID Controller");
  const [guideEnabled, setGuideEnabled] = useState(false);
  const [pidP, setPidP] = useState("1.2");
  const [pidI, setPidI] = useState("0.8");
  const [pidD, setPidD] = useState("0.4");
  const [adaptiveP, setAdaptiveP] = useState("1.5");
  const [adaptiveI, setAdaptiveI] = useState("0.9");
  const [adaptiveD, setAdaptiveD] = useState("0.3");
  const [adaptiveWindow, setAdaptiveWindow] = useState("10");
  const [feedforwardP, setFeedforwardP] = useState("2.0");
  const [feedforwardI, setFeedforwardI] = useState("1.1");
  const [feedforwardD, setFeedforwardD] = useState("0.5");
  const [sensitivity, setSensitivity] = useState([75]);
  const [cameraGain, setCameraGain] = useState("-1");
  const [cameraExposure, setCameraExposure] = useState("-1");
  const [cameraInterval, setCameraInterval] = useState("-1");
  const [cameraImageType, setCameraImageType] = useState(0); // default to RGB24
  const [mqttClient, setMqttClient] = useState<mqtt.MqttClient | null>(null);
  const [receivedImage, setReceivedImage] = useState<string | null>(null);
  const lastSetupPublish = useRef<number>(0);
  const [capturedName, setCapturedName] = useState("UNKNOWN");
  const [capturedExposure, setCapturedExposure] = useState("1000");
  const [capturedInterval, setCapturedInterval] = useState("30");
  const [capturedGain, setCapturedGain] = useState("150");
  const initializedRef = useRef(false);
  const offsetRef = useRef(offset);
  const zoomRef = useRef(zoom);
  const imageSizeRef = useRef({ width: 1280, height: 960 });
  const lastImageSrcRef = useRef(null);
  const initialSetupDoneRef = useRef(false);
  const lastMotorPublish = useRef(0);
  const stars = useRef([[2,3,1]]);
  const lastPublishedSpeeds = useRef({ ra: 0, dec: 0 });
  const [imageBuffer, setImageBuffer] = useState([]);
  const [selectedFormat, setSelectedFormat] = useState("jpg");
  const [current_expo, setCurrentExpo] = useState(0);
  const [total_expo, setTotalExpo] = useState(1000);
  const [gain_range, setGainRange] = useState([0,100]);
  const [exposure_range, setExpoRange] = useState([64,2000000000]);
  const [av_image_types, setImageTypes] = useState([0,1,2,3,4]);
  const [autoDump, setAutoDump] = useState(false);
  const deviceIP = window.location.hostname;
  const autoDumpRef = useRef(false);
var caputered_image_name = "UNK";

const imageTypeOptions = [
  { value: 0, label: "RGB24" },
  { value: 1, label: "RAW16" },
  { value: 2, label: "RAW8" },
  { value: 3, label: "Y8" },
  { value: 4, label: "Y16" }
];

  const ProgressBar = ({ current_expo, total_expo }) => {
    const percentage = Math.min(Math.floor((current_expo / total_expo) * 100), 100);
    return (
      <div className="w-full max-w-md">
        <div className="relative w-full bg-gray-200 rounded-full h-6 overflow-hidden shadow-inner">
          <div
            className="bg-blue-500 h-full transition-all duration-300 ease-in-out"
            style={{ width: `${percentage}%` }}
          ></div>
          <div className="absolute inset-0 flex items-center justify-center text-sm font-medium text-white">
            {current_expo} / {total_expo} ms
          </div>
        </div>
      </div>
    );
  };

useEffect(() => {
  caputered_image_name = capturedName;
  console.log("CapturedName updated:", capturedName);
}, [capturedName]);


  useEffect(() => {
    if (dec_speed_slider !== undefined) {
    }
  }, [dec_speed_slider]);

  useEffect(() => {
    if (ra_speed_slider !== undefined) {
    }
  }, [ra_speed_slider]);
  // MQTT Connection
  useEffect(() => {
    // Only try to connect if we're not on HTTPS or if we're on localhost
    const isSecure = window.location.protocol === "https:";
    const isLocalhost =
      window.location.hostname === "localhost" ||
      window.location.hostname === "127.0.0.1";

    if (isSecure && !isLocalhost) {
      console.warn(
        "MQTT connection skipped: Cannot connect to insecure WebSocket from HTTPS page",
      );
      return;
    }

    try {
      const client = mqtt.connect(`ws://${deviceIP}:9001`);

      client.on("connect", () => {
        console.log("MQTT connected");
        client.subscribe("/guider/image_png", (err) => {
          if (err) {
            console.error("Failed to subscribe to /guider/image_png:", err);
          } else {
            console.log("Subscribed to /guider/image_png");
          }
        });
        client.subscribe("/guider/image_jpg", (err) => {
          if (err) {
            console.error("Failed to subscribe to /guider/image_jpg:", err);
          } else {
            console.log("Subscribed to /guider/image_jpg");
          }
        });
        client.subscribe('guider/exposure_status', (err) => {
          if (err) {
            console.error("Failed to subscribe to guider/exposure_status", err);
          } else {
            console.log("Subscribed to guider/exposure_status");
          }
        });
        client.subscribe('guider/camera_info', (err) => {
          if (err) {
            console.error("Failed to subscribe to guider/camera_info", err);
          } else {
            console.log("Subscribed to guider/camera_info");
          }
        });
        client.subscribe('guider/image_info', (err) => {
          if (err) {
            console.error("Failed to subscribe to guider/image_info", err);
          } else {
            console.log("Subscribed to guider/image_info");
          }
        });
        client.subscribe('guider/detected_stars', (err) => {
          if (err) {
            console.error("Failed to subscribe to guider/image_info", err);
          } else {
            console.log("Subscribed to guider/detected_star");
          }
        });
      });

      client.on("message", (topic, message) => {
        if (topic === "/guider/image_jpg" || topic === "/guider/image_png") {
          try {
            const format = topic === "/guider/image_jpg" ? "jpg" : "png";
            setSelectedFormat(format);
            const base64 = btoa(
              new Uint8Array(message).reduce(
                (data, byte) => data + String.fromCharCode(byte),
                ""
              )
            );
            const imageUrl = `data:image/${format};base64,${base64}`;
            console.log(caputered_image_name)
            const imageEntry = { dataUrl: imageUrl, format, name: `${caputered_image_name}` };

            // Update image buffer (max 120)

              setImageBuffer(prevBuffer => {
                const newBuffer = [...prevBuffer, imageEntry];

                if (autoDumpRef.current && newBuffer.length >= 60) {
                  downloadImages(newBuffer); // pass buffer explicitly
                  return []; // clear buffer
                }

                if (!autoDumpRef.current && newBuffer.length > 360) {
                  newBuffer.shift(); // keep max 120
                }

                return newBuffer;
              });
            // setImageBuffer((prevBuffer) => {
            //   const newBuffer = [...prevBuffer, imageEntry];
            //   // keep max 360 images
            //   if (newBuffer.length > 360) newBuffer.shift();

            //   // auto-download if checkbox is checked and buffer exceeds threshold
            //   if (autoDump && newBuffer.length >= 60) {
            //     //downloadImages();   // your function to download all images
            //     return [];                   // clear buffer
            //   }

            //   return newBuffer;
            // });

            setReceivedImage(imageUrl);
          } catch (error) {
            console.error("Error processing received image:", error);
          }
        }
        if (topic === 'guider/exposure_status') {
          try {
            const data = JSON.parse(message.toString());
            if (data.curr_expo !== undefined && data.final_expo !== undefined) {
              setCurrentExpo(data.curr_expo);
              setTotalExpo(data.final_expo);
            }
          } catch (e) {
            console.error('Invalid JSON message:', e);
          }
        }
        if (topic === 'guider/detected_stars') {
          try {
            const data = JSON.parse(message.toString());
            stars.current = data.stars;   // update ref
            console.log(data)
          } catch (e) {
            console.error('Invalid JSON message:', e);
          }
        }
        if (topic === 'guider/camera_info') {
          try {
            const data = JSON.parse(message.toString());
            // console.log(data);
            if (data.gain_range !== undefined && data.exposure_range !== undefined && data.data_types !== undefined) {
              setGainRange(data.gain_range);
              setExpoRange(data.exposure_range);
              setImageTypes(data.data_types);
            }
          } catch (e) {
            console.error('Invalid JSON message:', e);
          }
        }
        if (topic === 'guider/image_info') {
          try {
            const data = JSON.parse(message.toString());
            console.log(data);

            if (
              data.gain !== undefined &&
              data.exposure !== undefined &&
              data.date !== undefined &&
              data.interval !== undefined
            ) {
              // always update "captured" values from MQTT
              setCapturedName("image" + String(data.ID))
              caputered_image_name = "image" + String(data.ID)
              setCapturedGain(data.gain);
              setCapturedExposure((data.exposure / 1000).toFixed(2));
              setCaptureTime(data.date);
              setCapturedInterval(data.interval);

              // only initialize input fields once (if user hasn't touched them yet)
              setCameraGain((prev) =>
                prev === "-1" ? String(data.gain) : prev
              );
              setCameraExposure((prev) =>
                prev === "-1" ? (data.exposure / 1000).toFixed(2) : prev
              );
              setCameraInterval((prev) =>
                prev === "-1" ? String(data.interval) : prev
              );
            }
          } catch (e) {
            console.error("Invalid JSON message:", e);
          }
        }
      });

      client.on("error", (err) => {
        console.error("MQTT connection error:", err);
      });

      setMqttClient(client);

      return () => {
        if (client) {
          client.end();
        }
      };
    } catch (error) {
      console.error("Failed to create MQTT client:", error);
    }
  }, []);

  // // Generate random data points for visualization
  // useEffect(() => {
  //   const generateDataPoints = () => {
  //     const points: DataPoint[] = [];
  //     for (let i = 0; i < 45; i++) {
  //       points.push({
  //         x: Math.random() * 800,
  //         y: Math.random() * 600,
  //         type:
  //           Math.random() > 0.7
  //             ? "target"
  //             : Math.random() > 0.5
  //               ? "reference"
  //               : Math.random() > 0.3
  //                 ? "variable"
  //                 : "artifact",
  //         magnitude: Math.random() * 15 + 5,
  //         id: `star_${i}`,
  //       });
  //     }
  //     setDataPoints(points);
  //   };

  //   generateDataPoints();
  //   const interval = setDecSpeed(generateDataPoints, 5000);
  //   return () => clearInterval(interval);
  // }, []);


  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas || !receivedImage) return;

    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    const img = new Image();
    img.onload = () => {
      imageSizeRef.current = { width: img.width, height: img.height };

      const canvasWidth = canvas.width;
      const canvasHeight = canvas.height;

      const scale = Math.min(canvasWidth / img.width, canvasHeight / img.height);
      minZoomRef.current = scale;

      if (!initialSetupDoneRef.current) {
        // First time only: set zoom & offset
        const initialOffsetX = (canvasWidth - img.width * scale) / 2;
        const initialOffsetY = (canvasHeight - img.height * scale) / 2;

        setZoom(scale);
        setOffset({ x: initialOffsetX, y: initialOffsetY });

        initialSetupDoneRef.current = true;
      }

      // Draw image
      ctx.imageSmoothingEnabled = false;
      ctx.mozImageSmoothingEnabled = false;    // Firefox
      ctx.webkitImageSmoothingEnabled = false; // Safari
      ctx.msImageSmoothingEnabled = false;     // IE
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      ctx.save();
      ctx.setTransform(zoom, 0, 0, zoom, offset.x, offset.y);
      ctx.drawImage(img, 0, 0);
      ctx.strokeStyle = "red";
      ctx.lineWidth = 2 / zoom; // scale line width so it stays consistent when zooming
      ctx.strokeStyle = "red";
      ctx.lineWidth = 2 / zoom; // scale line width so it stays consistent when zooming
      console.log(stars.current)
      stars.current.forEach(([cx, cy, area]) => {
        ctx.beginPath();
        ctx.arc(cx, cy, 5, 0, 2 * Math.PI);  // or area if you want radius proportional
        ctx.stroke();
      });
      ctx.restore();
    };

    img.src = receivedImage;

    // No cleanup resetting initialSetupDoneRef here!
  }, [receivedImage, zoom, offset]);


  useEffect(() => {
    if (mqttClient?.connected) {
      mqttClient.publish("/guider/format", selectedFormat);
    }
    console.log("Published format")
  }, [selectedFormat, mqttClient]);
  useEffect(() => {
  // If the current selection is no longer available, pick the first available type
  if (!av_image_types.includes(cameraImageType) && av_image_types.length > 0) {
    setCameraImageType(av_image_types[0]);
  }
}, [av_image_types, cameraImageType]);
  // Draw grid and data points only when no image is received
  useEffect(() => {
    if (receivedImage) return; // Don't draw grid if we have an image

    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    // Clear canvas
    ctx.fillStyle = "#0a0a0a";
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    // Draw grid
    ctx.strokeStyle = "#333333";
    ctx.lineWidth = 0.5;

    // Vertical lines
    for (let x = 0; x <= canvas.width; x += 40) {
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, canvas.height);
      ctx.stroke();
    }

    // Horizontal lines
    for (let y = 0; y <= canvas.height; y += 40) {
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(canvas.width, y);
      ctx.stroke();
    }

    // Draw major grid lines
    ctx.strokeStyle = "#555555";
    ctx.lineWidth = 1;
    for (let x = 0; x <= canvas.width; x += 120) {
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, canvas.height);
      ctx.stroke();
    }
    for (let y = 0; y <= canvas.height; y += 120) {
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(canvas.width, y);
      ctx.stroke();
    }

    // Draw data points
    dataPoints.forEach((point) => {
      const size = Math.max(2, (20 - point.magnitude) / 2);

      ctx.beginPath();
      ctx.arc(point.x, point.y, size, 0, 2 * Math.PI);

      switch (point.type) {
        case "target":
          ctx.fillStyle = "#00ff00";
          break;
        case "reference":
          ctx.fillStyle = "#0088ff";
          break;
        case "variable":
          ctx.fillStyle = "#ffaa00";
          break;
        case "artifact":
          ctx.fillStyle = "#ff4444";
          break;
      }

      ctx.fill();

      // Add glow effect for brighter stars
      if (point.magnitude < 10) {
        ctx.beginPath();
        ctx.arc(point.x, point.y, size + 2, 0, 2 * Math.PI);
        ctx.fillStyle = ctx.fillStyle + "40";
        ctx.fill();
      }
    });

    // Draw crosshair in center
    const centerX = canvas.width / 2;
    const centerY = canvas.height / 2;
    ctx.strokeStyle = "#ff6600";
    ctx.lineWidth = 2;

    ctx.beginPath();
    ctx.moveTo(centerX - 20, centerY);
    ctx.lineTo(centerX + 20, centerY);
    ctx.moveTo(centerX, centerY - 20);
    ctx.lineTo(centerX, centerY + 20);
    ctx.stroke();
  }, [dataPoints, receivedImage]);

  const handleCapture = () => {
    setIsCapturing(!isCapturing);
    if (!isCapturing) {
      setObservatoryStatus("CAPTURING");
    } else {
      setObservatoryStatus("READY");
    }
  };


const downloadImages = async (images = null, filename = "images.zip") => {
  const zip = new JSZip();

  const source = images ?? imageBuffer; // if null, fall back to imageBuffer

  source.forEach((img) => {
    const base64 = img.dataUrl.split(",")[1];
    const ext = img.format;
    zip.file(`${img.name}.${ext}`, base64, { base64: true });
  });

  const content = await zip.generateAsync({ type: "blob" });
  saveAs(content, filename);
};
  const handleSetup = () => {
    const now = Date.now();

    // Rate limit to 100ms
    if (now - lastSetupPublish.current < 200) {
      return;
    }

    if (mqttClient && mqttClient.connected) {
      const cameraControlData = {
        gain: parseInt(cameraGain) || 0,
        exp: Math.round(parseFloat(cameraExposure) * 1000) || 0,  // <-- changed line
        inter: parseInt(cameraInterval) || 0,
        data_type: cameraImageType,
      };

      const jsonString = JSON.stringify(cameraControlData);

      mqttClient.publish("guider/camera_controls", jsonString, (err) => {
        if (err) {
          console.error("Failed to publish camera controls:", err);
        } else {
          console.log("Camera controls published:", jsonString);
        }
      });
    } else {
      console.warn("MQTT client not connected, cannot publish camera controls");
    }
    lastSetupPublish.current = now;
  };

  const handleCanvasClick = (event: React.MouseEvent<HTMLCanvasElement>) => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const rect = canvas.getBoundingClientRect();

    // Adjust for CSS scaling
    const scaleX = canvas.width / rect.width;
    const scaleY = canvas.height / rect.height;

    // Mouse position in canvas coordinate system (pixels)
    const mouseX = (event.clientX - rect.left) * scaleX;
    const mouseY = (event.clientY - rect.top) * scaleY;

    // Reverse zoom & offset transform to get original image coords
    const imgX = (mouseX - offset.x) / zoom;
    const imgY = (mouseY - offset.y) / zoom;

    const { width, height } = imageSizeRef.current;

    if (imgX >= 0 && imgX <= width && imgY >= 0 && imgY <= height) {
      console.log(`Clicked on image at original coords: (${imgX.toFixed(2)}, ${imgY.toFixed(2)})`);
    } else {
      console.log("Clicked outside image boundaries");
    }
  };

  const handleWheel = (e: React.WheelEvent<HTMLCanvasElement>) => {
    e.preventDefault();
    const canvas = canvasRef.current;
    if (!canvas) return;

    const rect = canvas.getBoundingClientRect();
    const mouseX = e.clientX - rect.left;
    const mouseY = e.clientY - rect.top;

    const zoomFactor = 0.1;
    const delta = e.deltaY < 0 ? 1 + zoomFactor : 1 - zoomFactor;
    const newZoom = Math.max(zoom * delta, minZoomRef.current); // limit zoom out

    const newOffsetX = mouseX - (mouseX - offset.x) * (newZoom / zoom);
    const newOffsetY = mouseY - (mouseY - offset.y) * (newZoom / zoom);

    setZoom(newZoom);
    setOffset({ x: newOffsetX, y: newOffsetY });
  };

  const handleResetZoom = () => {
    const canvas = canvasRef.current;
    if (!canvas || !receivedImage) return;

    const img = new Image();
    img.onload = () => {
      const canvasWidth = canvas.width;
      const canvasHeight = canvas.height;
      const scale = Math.min(canvasWidth / img.width, canvasHeight / img.height);
      const offsetX = (canvasWidth - img.width * scale) / 2;
      const offsetY = (canvasHeight - img.height * scale) / 2;

      setZoom(scale);
      setOffset({ x: offsetX, y: offsetY });
    };
    img.src = receivedImage;
  };


  useEffect(() => {
    const interval = setInterval(() => {
      const now = Date.now();

      // Only publish if at least 100ms passed AND speeds changed
      if (
        now - lastMotorPublish.current >= 100 &&
        (lastPublishedSpeeds.current.ra !== ra_speed_slider[0] ||
          lastPublishedSpeeds.current.dec !== dec_speed_slider[0])
      ) {
        if (mqttClient && mqttClient.connected) {
          const motorData = {
            ra_speed: ra_speed_slider[0] || 0,
            dec_speed: dec_speed_slider[0] || 0,
          };
          const jsonString = JSON.stringify(motorData);

          mqttClient.publish("guider/motors", jsonString, (err) => {
            if (err) {
              console.error("Failed to publish motor speeds:", err);
            } else {
              console.log("Motor speeds published:", jsonString);
            }
          });

          lastMotorPublish.current = now;
          lastPublishedSpeeds.current = {
            ra: ra_speed_slider[0],
            dec: dec_speed_slider[0],
          };
        }
      }
    }, 100);

    return () => clearInterval(interval);
  }, [ra_speed_slider, dec_speed_slider, mqttClient]);


  const handleRASpeedChange = (value: number[]) => {
    setRaSpeed(value);
  };

  const handleDecSpeedChange = (value: number[]) => {
    setDecSpeed(value);
  };

  return (
    <div className="min-h-screen bg-zinc-800 text-gray-400">
      <div className="container mx-auto p-3 h-[calc(100vh-20px)]">
        <div className="grid grid-cols-12 gap-3 h-full">
          {/* Left Control Panel */}
          <div className="col-span-12 lg:col-span-2 space-y-3">
            {/* Control Tabs */}
            <Card className="bg-zinc-900 text-gray-400 border-gray-600 shadow-sm">
              <CardContent className="p-3">
                <Tabs defaultValue="manual" className="w-full bg-zinc-900">
                  <TabsList className="grid w-full grid-cols-2 mb-3 bg-zinc-900">
                    <TabsTrigger value="manual"     className="text-xs text-gray-400 bg-zinc-900 border border-gray-600 rounded-l-md
                                                                  data-[state=active]:bg-gray-600 data-[state=active]:text-gray-400">
                      Manual
                    </TabsTrigger>
                    <TabsTrigger value="auto" className="text-xs text-gray-400 bg-zinc-900 border border-gray-600 rounded-l-md
                                                                  data-[state=active]:bg-gray-600 data-[state=active]:text-gray-400">
                      Auto
                    </TabsTrigger>
                  </TabsList>

                  <TabsContent value="manual" className="space-y-3 mt-0">
                    <div className="space-y-3">
                      <div className="space-y-2">
                        <div className="flex items-center justify-between">
                          <Label className="text-xs text-gray-400">
                            RA Speed
                          </Label>
                          <span className="text-xs text-gray-400">
                            {(ra_speed_slider && ra_speed_slider[0]) || 0}"/s
                          </span>
                        </div>
                                <Slider
                          value={ra_speed_slider || [0]}
                          onValueChange={handleRASpeedChange}
                          max={500}
                          min={-500}
                          step={10}
                          className="w-full bg-gray-700 h-1 rounded"
                        />
                      </div>

                      <div className="space-y-2">
                        <div className="flex items-center justify-between">
                          <Label className="text-xs text-gray-400">
                            Dec Speed
                          </Label>
                          <span className="text-xs text-gray-400">
                            {(dec_speed_slider && dec_speed_slider[0]) || 0}"/s
                          </span>
                        </div>
                        <Slider
                          value={dec_speed_slider || [0]}
                          onValueChange={handleDecSpeedChange}
                          max={500}
                          min={-500}
                          step={10}
                          className="w-full"
                        />
                      </div>
                    </div>
                  </TabsContent>

                  <TabsContent value="auto" className="space-y-3 mt-0">
                    <div className="space-y-2">
                      <div className="flex items-center justify-between">
                        <Label className="text-xs text-gray-600">
                          Control Algorithm
                        </Label>
                        <Button
                          variant={guideEnabled ? "default" : "outline"}
                          size="sm"
                          className="text-xs h-6 px-2"
                          onClick={() => setGuideEnabled(!guideEnabled)}
                        >
                          Guide {guideEnabled ? "ON" : "OFF"}
                        </Button>
                      </div>
                      <select
                        className="w-full h-7 text-xs border border-gray-600 rounded px-2 bg-zinc-900 text-gray-400"
                        value={selectedAlgorithm}
                        onChange={(e) => setSelectedAlgorithm(e.target.value)}
                      >
                        <option>PID Controller</option>
                        <option>Adaptive Control</option>
                        <option>Feedforward</option>
                      </select>
                    </div>

                    {/* Dynamic Algorithm Controls */}
                    {selectedAlgorithm === "PID Controller" && (
                      <div className="space-y-2">
                        <div className="space-y-1">
                          <Label className="text-xs text-gray-600">
                            P Gain
                          </Label>
                          <Input
                            value={pidP}
                            onChange={(e) => setPidP(e.target.value)}
                            className="h-6 text-xs border-gray-600"
                          />
                        </div>
                        <div className="space-y-1">
                          <Label className="text-xs text-gray-600">
                            I Gain
                          </Label>
                          <Input
                            value={pidI}
                            onChange={(e) => setPidI(e.target.value)}
                            className="h-6 text-xs border-gray-600"
                          />
                        </div>
                        <div className="space-y-1">
                          <Label className="text-xs text-gray-600">
                            D Gain
                          </Label>
                          <Input
                            value={pidD}
                            onChange={(e) => setPidD(e.target.value)}
                            className="h-6 text-xs border-gray-600"
                          />
                        </div>
                      </div>
                    )}

                    {selectedAlgorithm === "Adaptive Control" && (
                      <div className="space-y-2">
                        <div className="space-y-1">
                          <Label className="text-xs text-gray-600">
                            P Gain
                          </Label>
                          <Input
                            value={adaptiveP}
                            onChange={(e) => setAdaptiveP(e.target.value)}
                            className="h-6 text-xs border-gray-600"
                          />
                        </div>
                        <div className="space-y-1">
                          <Label className="text-xs text-gray-600">
                            I Gain
                          </Label>
                          <Input
                            value={adaptiveI}
                            onChange={(e) => setAdaptiveI(e.target.value)}
                            className="h-6 text-xs border-gray-600"
                          />
                        </div>
                        <div className="space-y-1">
                          <Label className="text-xs text-gray-600">
                            D Gain
                          </Label>
                          <Input
                            value={adaptiveD}
                            onChange={(e) => setAdaptiveD(e.target.value)}
                            className="h-6 text-xs border-gray-600"
                          />
                        </div>
                        <div className="space-y-1">
                          <Label className="text-xs text-gray-600">
                            Window Size
                          </Label>
                          <Input
                            value={adaptiveWindow}
                            onChange={(e) => setAdaptiveWindow(e.target.value)}
                            className="h-6 text-xs border-gray-600"
                          />
                        </div>
                      </div>
                    )}

                    {selectedAlgorithm === "Feedforward" && (
                      <div className="space-y-2">
                        <div className="space-y-1">
                          <Label className="text-xs text-gray-600">
                            P Gain
                          </Label>
                          <Input
                            value={feedforwardP}
                            onChange={(e) => setFeedforwardP(e.target.value)}
                            className="h-6 text-xs border-gray-600"
                          />
                        </div>
                        <div className="space-y-1">
                          <Label className="text-xs text-gray-600">
                            I Gain
                          </Label>
                          <Input
                            value={feedforwardI}
                            onChange={(e) => setFeedforwardI(e.target.value)}
                            className="h-6 text-xs border-gray-600"
                          />
                        </div>
                        <div className="space-y-1">
                          <Label className="text-xs text-gray-600">
                            D Gain
                          </Label>
                          <Input
                            value={feedforwardD}
                            onChange={(e) => setFeedforwardD(e.target.value)}
                            className="h-6 text-xs border-gray-600"
                          />
                        </div>
                      </div>
                    )}
                  </TabsContent>
                </Tabs>
              </CardContent>
            </Card>

            {/* Camera Settings */}
            <Card className="bg-zinc-900 text-gray-400 border-gray-600 shadow-sm">
              <CardHeader className="pb-3">
                <CardTitle className="text-sm text-gray-400">
                  Captured Image
                </CardTitle>
              </CardHeader>
              <CardContent className="space-y-4">
                <div className="space-y-2">
                  <Label className="text-xs text-gray-400">Title</Label>
                  <Input
                    placeholder="Image title"
                    className="h-7 text-xs border-gray-600 text-zinc-800 bg-gray-500"
                  />
                </div>

                <div className="space-y-2">
                  <Label className="text-xs text-gray-400">Exposure</Label>
                  <Input
                    value={capturedExposure}
                    onChange={(e) => setCapturedExposure(e.target.value)}
                    readOnly
                    className="h-7 text-xs border-gray-600 text-zinc-800 bg-gray-500"
                  />
                </div>

                <div className="space-y-2">
                  <Label className="text-xs text-gray-400">Capture Time</Label>
                  <Input
                    value={captureTime}
                    onChange={(e) => setCaptureTime(e.target.value)}
                    readOnly
                    className="h-7 text-xs border-gray-600 text-zinc-800 bg-gray-500"
                  />
                </div>

                <div className="space-y-2">
                  <Label className="text-xs text-gray-400">Interval</Label>
                  <Input
                    value={capturedInterval}
                    onChange={(e) => setCapturedInterval(e.target.value)}
                    readOnly
                    className="h-7 text-xs border-gray-600 text-zinc-800 bg-gray-500"
                  />
                </div>

                <div className="space-y-2">
                  <Label className="text-xs text-gray-400">Gain</Label>
                  <Input
                    value={capturedGain}
                    onChange={(e) => setCapturedGain(e.target.value)}
                    className="h-7 text-xs border-gray-600 text-zinc-800 bg-gray-500"
                  />
                </div>
              </CardContent>
            </Card>
          </div>

          {/* Main Visualization Area */}
          <div className="col-span-12 lg:col-span-8 space-y-3">
            {/* Analysis Tabs */}
            <Card className="bg-zinc-900 text-gray-400 border-gray-600 shadow-sm h-36">
              <CardContent className="p-2 h-full">
                <Tabs defaultValue="spectrum" className="h-full flex flex-col bg-zinc-900">
                  <TabsList className="grid w-full grid-cols-3 mb-2 h-7 flex-shrink-0  bg-zinc-900">
                    <TabsTrigger
                      value="spectrum"
                      className="text-xs text-gray-400 bg-zinc-900 data-[state=active]:bg-gray-600 data-[state=active]:text-gray-400
                      focus:outline-none focus:ring-2 focus:ring-gray-700"
                    >
                      Spectrum
                    </TabsTrigger>
                    <TabsTrigger value="signal"
                     className="text-xs text-gray-400 bg-zinc-900 data-[state=active]:bg-gray-600 data-[state=active]:text-gray-400">
                      Signal
                    </TabsTrigger>
                    <TabsTrigger value="options"
                     className="text-xs text-gray-400 bg-zinc-900 data-[state=active]:bg-gray-600 data-[state=active]:text-gray-400 border-gray-600">
                      Options
                    </TabsTrigger>
                  </TabsList>

                  <TabsContent
                    value="spectrum"
                    className="flex-1 mt-0 overflow-hidden bg-zinc-800 border-gray-600 rounded-lg"
                  >
                    <div className="grid grid-cols-2 gap-2 h-full">
                      <div className="bg-black rounded border flex items-center justify-center min-h-0">
                        <div className="text-xs text-gray-500 flex items-center">
                          <BarChart3 className="h-3 w-3 mr-1" />
                          FFT
                        </div>
                      </div>
                      <div className="bg-black rounded border flex items-center justify-center min-h-0">
                        <div className="text-xs text-gray-500 flex items-center">
                          <Activity className="h-3 w-3 mr-1" />
                          Histogram
                        </div>
                      </div>
                    </div>
                  </TabsContent>

                  <TabsContent
                    value="signal"
                    className="flex-1 mt-0 overflow-hidden  bg-zinc-800 rounded-lg"
                  >
                    <div className="grid grid-cols-2 gap-2 h-full">
                      <div className="bg-black rounded border flex items-center justify-center min-h-0">
                        <div className="text-xs text-gray-500 flex items-center">
                          <Activity className="h-3 w-3 mr-1" />
                          Amplitude
                        </div>
                      </div>
                      <div className="bg-black rounded border flex items-center justify-center min-h-0">
                        <div className="text-xs text-gray-500 flex items-center">
                          <BarChart3 className="h-3 w-3 mr-1" />
                          Phase
                        </div>
                      </div>
                    </div>
                  </TabsContent>

                  <TabsContent
                    value="options"
                    className="flex-1 mt-0 overflow-hidden  bg-zinc-800  border-gray-600 rounded-lg"
                  >
                    <div className="grid grid-cols-3 gap-2 h-full">
                      {/* Column 1 */}
                      <div className="space-y-1">
                        <div className="flex items-center space-x-2">
                          <Checkbox
                            id="autoScale"
                            className="data-[state=checked]:bg-blue-600  bg-gray-500"
                          />
                          <Label htmlFor="autoScale" className="text-xs">
                            Auto scale
                          </Label>
                        </div>
                        <div className="flex items-center space-x-2">
                          <Checkbox
                            id="logScale"
                            className="data-[state=checked]:bg-blue-600  bg-gray-500"
                          />
                          <Label htmlFor="logScale" className="text-xs">
                            Log scale
                          </Label>
                        </div>
                      </div>

                      {/* Column 2 */}
                      <div className="space-y-1">
                        <div className="flex items-center space-x-2">
                          <Checkbox
                            id="realTimeUpdate"
                            defaultChecked
                            className="data-[state=checked]:bg-blue-600  bg-gray-500"
                          />
                          <Label htmlFor="realTimeUpdate" className="text-xs">
                            Real-time update
                          </Label>
                        </div>
                        <div className="flex items-center space-x-2">
                          <Checkbox
                            id="enableCompression2"
                            checked={enableCompression}
                            onCheckedChange={setEnableCompression}
                            className="data-[state=checked]:bg-blue-600  bg-gray-500"
                          />
                          <Label
                            htmlFor="enableCompression2"
                            className="text-xs"
                          >
                            Enable compression
                          </Label>
                        </div>
                      </div>

                      {/* Column 3 */}
                      <div className="space-y-1">
                        <div className="flex items-center space-x-2">
                          <Checkbox
                            id="showGridLines2"
                            defaultChecked
                            className="data-[state=checked]:bg-blue-600  bg-gray-500"
                          />
                          <Label htmlFor="showGridLines2" className="text-xs">
                            Show grid lines
                          </Label>
                        </div>
                        <div className="flex items-center space-x-2">
                          <Checkbox
                            id="saveRaw2"
                            checked={saveRaw}
                            onCheckedChange={setSaveRaw}
                            className="data-[state=checked]:bg-blue-600 bg-gray-500"
                          />
                          <Label htmlFor="saveRaw2" className="text-xs">
                            Save Raw
                          </Label>
                        </div>

                        <div className="space-y-1 mt-2">
                          <div className="flex items-center justify-between">
                            <Label className="text-xs text-gray-400">
                              Sensitivity
                            </Label>
                            <span className="text-xs text-gray-400">
                              {(sensitivity && sensitivity[0]) || 75}%
                            </span>
                          </div>
                          <Slider
                            value={sensitivity || [75]}
                            onValueChange={setSensitivity}
                            max={100}
                            min={0}
                            step={5}
                            className="w-full"
                          />
                        </div>
                      </div>
                    </div>
                  </TabsContent>
                </Tabs>
              </CardContent>
            </Card>

            {/* Main Observation Display */}
            <Card className="flex-1 bg-zinc-900 text-gray-400 border-gray-600 shadow-sm">
              <CardHeader className="pb-2">
                <div className="flex items-center justify-between">
                  <CardTitle className="text-sm text-gray-400">
                    Observed Image
                  </CardTitle>
                  <div className="flex items-center space-x-4">
                    <div className="text-xs text-gray-500">
                      <button
                        onClick={() => setSelectedFormat("jpg")}
                        className={`px-3 py-1 text-xs rounded border border-gray-600 ${selectedFormat === "jpg"
                            ? "text-gray-400 bg-gray-600"
                            : "text-gray-600 bg-zinc-800 hover:bg-gray-700"
                          }`}
                      >
                        JPEG
                      </button>

                      <button
                        onClick={() => setSelectedFormat("png")}
                        className={`px-3 py-1 text-xs rounded border border-gray-600 ${selectedFormat === "png"
                            ? "text-gray-400 bg-gray-600"
                            : "text-gray-600 bg-zinc-800 hover:bg-gray-700"
                          }`}
                      >
                        PNG
                      </button>
                    </div>
                    <button
                      onClick={handleResetZoom}
                      className="px-2 py-1 text-xs text-gray-400 bg-zinc-800 hover:bg-gray-800 rounded"
                    >
                      Reset Zoom
                    </button>
                    <button
                      onClick={() => downloadImages()}
                      disabled={imageBuffer.length === 0}
                      className="px-2 py-1 text-xs text-gray-400 bg-zinc-800 hover:bg-gray-800 rounded disabled:opacity-50 disabled:cursor-not-allowed"
                    >
                      Download Images
                    </button>
                        <Checkbox
                      id="autoDump"
                      className="data-[state=checked]:bg-blue-600  bg-gray-500"
                      checked={autoDumpRef.current}
                      onCheckedChange={(checked) => autoDumpRef.current = checked}
                    />
                    <Label htmlFor="autoDump" className="text-xs">Auto dump</Label>
                    <span className="text-xs text-gray-400">
                      {imageBuffer.length} image{imageBuffer.length !== 1 && "s"} in buffer
                    </span>

                    <button
                      onClick={() => setImageBuffer([])}
                      disabled={imageBuffer.length === 0}
                      className="px-2 py-1 text-xs text-red-600 bg-red-100 hover:bg-red-200 rounded disabled:opacity-50 disabled:cursor-not-allowed"
                    >
                      Reset Buffer
                    </button>
                  </div>
                </div>
              </CardHeader>
              <CardContent className="p-3">
                <div className="w-full aspect-[4/3]">
                  <canvas
                    ref={canvasRef}
                    onWheel={handleWheel}
                    width={800}
                    height={600}
                    className="w-full h-full bg-black border border-gray-400 rounded"
                    onClick={handleCanvasClick}
                  />
                </div>
              </CardContent>
            </Card>
          </div>

          {/* Right Status Panel */}
          <div className="col-span-12 lg:col-span-2 space-y-3">
            {/* Battery Status */}
            <Card className="bg-zinc-900 text-gray-400 border-gray-600 shadow-sm">
              <CardHeader className="pb-3">
                <CardTitle className="text-sm text-gray-400 flex items-center">
                  Battery
                </CardTitle>
              </CardHeader>
              <CardContent className="space-y-3">
                <div className="space-y-2">
                  <div className="flex justify-between text-sm">
                    <span className="text-gray-400">Voltage</span>
                    <span className="font-mono font-semibold text-gray-400">
                      {voltage}V
                    </span>
                  </div>
                  <div className="flex justify-between text-sm">
                    <span className="text-gray-400">Current</span>
                    <span className="font-mono font-semibold text-gray-400">
                      {current}A
                    </span>
                  </div>
                </div>

                {/* Battery State - 10 Elements */}
                <div className="grid grid-cols-10 gap-1 mt-3">
                  <div className="h-2 bg-green-400 rounded-sm"></div>
                  <div className="h-2 bg-green-400 rounded-sm"></div>
                  <div className="h-2 bg-green-400 rounded-sm"></div>
                  <div className="h-2 bg-green-400 rounded-sm"></div>
                  <div className="h-2 bg-green-400 rounded-sm"></div>
                  <div className="h-2 bg-green-400 rounded-sm"></div>
                  <div className="h-2 bg-green-400 rounded-sm"></div>
                  <div className="h-2 bg-yellow-400 rounded-sm"></div>
                  <div className="h-2 bg-yellow-400 rounded-sm"></div>
                  <div className="h-2 bg-red-400 rounded-sm"></div>
                </div>
              </CardContent>
            </Card>

            {/* Camera Configuration */}
            <Card className="bg-zinc-900 text-gray-400 border-gray-700 shadow-md">
              <CardHeader className="pb-3">
                <CardTitle className="text-sm">
                  Camera Configuration
                </CardTitle>
              </CardHeader>
              <CardContent className="space-y-3">
                <div className="space-y-2">
                  <Label className="text-xs">Image Type</Label>
                  <select
                    className="h-7 text-xs  text-zinc-800 bg-gray-500 border-gray-600 rounded w-full"
                    value={cameraImageType}
                    onChange={(e) => setCameraImageType(parseInt(e.target.value))}
                  >
                    {imageTypeOptions
                      .filter(opt => av_image_types.includes(opt.value)) // only include available types
                      .map(opt => (
                        <option key={opt.value} value={opt.value}>
                          {opt.label}
                        </option>
                      ))
                    }
                  </select>
                </div>
                <div className="space-y-2">
                  <Label className="text-xs">Gain {gain_range ? `(${gain_range[0]} - ${gain_range[1]})` : ""} </Label>
                  <Input
                    type="number"
                    inputMode="numeric"
                    pattern="[0-9]*"
                    value={cameraGain}
                    onChange={(e) => setCameraGain(e.target.value)}
                    onKeyDown={(e) => e.key === 'Enter' && handleSetup()}
                    className="h-7 text-xs  text-zinc-800 bg-gray-500 border-gray-600"
                    placeholder="150"
                  />
                </div>
                <div className="space-y-2">
                  <Label className="text-xs">Exposure (ms)</Label>
                  <Input
                    type="text"
                    inputMode="decimal"
                    pattern="[0-9.]*"
                    value={cameraExposure}
                    onChange={(e) => {
                      const newExposure = e.target.value;
                      setCameraExposure(newExposure);

                      const exposureMs = parseFloat(newExposure);
                      const currentInterval = parseInt(cameraInterval) || 0;

                      if (!isNaN(exposureMs) && exposureMs > currentInterval) {
                        setCameraInterval(String(Math.round(exposureMs)));
                      }
                    }}
                    onKeyDown={(e) => e.key === 'Enter' && handleSetup()}
                    className="h-7 text-xs  text-zinc-800 bg-gray-500 border-gray-600"
                    placeholder="1.0"
                  />
                </div>
                <div className="space-y-2">
                  <Label className="text-xs">Interval (ms)</Label>
                  <Input
                    type="number"
                    inputMode="numeric"
                    pattern="[0-9]*"
                    value={cameraInterval}
                    onChange={(e) => {
                      const newInterval = e.target.value;
                      setCameraInterval(newInterval);

                      const inter = parseFloat(newInterval);
                      const exp = parseFloat(cameraExposure);

                      if (!isNaN(inter) && !isNaN(exp) && exp > inter) {
                        setCameraExposure(String(inter));
                      }
                    }}
                    onKeyDown={(e) => e.key === 'Enter' && handleSetup()}
                    className="h-7 text-xs  text-zinc-800 bg-gray-500 border-gray-600"
                    placeholder="30"
                  />
                </div>
                <Button
                  variant="default"
                  size="sm"
                  className="w-full text-xs h-7 mt-3 text-gray-400 bg-zinc-800 hover:bg-gray-800"
                  onClick={handleSetup}
                >
                  Setup
                </Button>
                <div className="p-4">
                  <h1 className="mb-4 text-xl font-semibold">Progress</h1>
                  <ProgressBar current_expo={current_expo} total_expo={total_expo} />
                </div>
              </CardContent>
            </Card>

            {/* Detection Summary */}
            <Card className="bg-zinc-900 text-gray-400 border-gray-600 shadow-sm">
              <CardHeader className="pb-3">
                <CardTitle className="text-sm text-gray-700">
                  Detection Summary
                </CardTitle>
              </CardHeader>
              <CardContent className="space-y-3">
                <div className="space-y-2">
                  <div className="flex items-center justify-between">
                    <div className="flex items-center gap-2">
                      <div className="w-3 h-3 bg-green-500 rounded-full"></div>
                      <span className="text-xs text-gray-700">Targets</span>
                    </div>
                    <span className="text-xs font-mono font-semibold">
                      {dataPoints.filter((p) => p.type === "target").length}
                    </span>
                  </div>
                  <div className="flex items-center justify-between">
                    <div className="flex items-center gap-2">
                      <div className="w-3 h-3 bg-blue-500 rounded-full"></div>
                      <span className="text-xs text-gray-700">References</span>
                    </div>
                    <span className="text-xs font-mono font-semibold">
                      {dataPoints.filter((p) => p.type === "reference").length}
                    </span>
                  </div>
                </div>
              </CardContent>
            </Card>
          </div>
        </div>
      </div>
    </div>
  );
}
