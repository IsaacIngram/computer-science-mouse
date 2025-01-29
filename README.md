# Computer Science Mouse
A software-based solution to your mouse problems, Computer Science Mouse is a smart, IoT enabled mousetrap.

# Local Setup
## 1. Download and Install ESP-IDF
This project is built using ESP-IDF, and you will need that installed on your system before you can proceed.

## 2. Add Twilio Cert PEM File
In order to make requests to Twilio, you will need to add a Cert PEM File. This belongs in
```
<path>/<to>/<repo>/dashboard/data/twilio_cert.pem
```

You can generate the file using the following command:
```
echo | openssl s_client -connect api.twilio.com:443 | openssl x509 > twilio_cert.pem
```

## 3. Configure ESP-IDF for this target
### 3.1 Enable Multiple Partitions
The `twilio_cert.pem` file is loaded into a partition and read via SPIFFS. To set up this partition, you must do
```
idf.py menuconfig
```
Then navigate to `Partition Table` and click `Partition Table (Single factory app, no OTA)`. Change this to `Custom partition table CSV`, which should just be set to `partitions.csv`.

