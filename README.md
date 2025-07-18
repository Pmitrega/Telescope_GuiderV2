## Development Environment setup

Setup docker:
<pre>
sudo apt install -y docker.io
sudo usermod -aG docker $USER
exec su -l $USER
docker pull --platform linux/arm64 debian:11
</pre>

Enable multiarchitecture emulation and verify it:
<pre>
docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
docker run --rm --privileged multiarch/qemu-user-static --version
</pre>

Now run crosscompile script, first it setup container with :
<pre>
./cross_compile.sh
</pre>

## Target Environment setup
Setup MQTT broker
<pre>
sudo apt update
sudo apt install -y mosquitto mosquitto-clients
</pre>
Enable mosquitto as service
<pre>
sudo systemctl enable mosquitto
sudo systemctl start mosquitto
sudo systemctl status mosquitto
</pre>
To test broker on one console run:
<pre>
mosquitto_sub -t test/topic
</pre>
On other use, after this message on previous one shoud show.
<pre>
mosquitto_pub -t test/topic -m "Hello MQTT"
</pre>

### Mosquito configuration
Now enable websockets and free access:
<pre>
sudo nano /etc/mosquitto/mosquitto.conf
</pre>
Add following lines:
<pre>
listener 1883
allow_anonymous true
listener 9001
</pre>

