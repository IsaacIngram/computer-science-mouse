# Computer Science Mouse
A software-based solution to your mouse problems, Computer Science Mouse is a smart, IoT enabled mousetrap.

# Local Setup
## 1. Download and Install ESP-IDF
This project is built using ESP-IDF, and you will need that installed on your system before you can proceed.

## 2. Activate Your ESP-IDF Build Environment
```
. /esp/esp-idf/export.sh # Assuming you installed ESP-IDF into /esp/esp-idf/
```

## 3. Configure TLS (ONLY IF YOU TRUST YOUR NETWORK)
In order to make requests to Twilio, you will need to add a Cert PEM file or you can disable certificate verification. I tried setting up the Twilio cert PEM file but got tired of getting issues from SPIFFS, so in the meantime I have decided to just disable verification. If you know how to fix my SPIFFS problems, feel free to send me a message or make a PR!

To disable certificate verification, open menuconfig
```
idf.py menuconfig
```
Navigate to `Component Config -> ESP-TLS`, and then make sure the following options are selected:
1. Allow potentially insecure options
2. Skip server certificate verification by default

## 3. Setup Secrets
In the main directory (`/path/to/repo/dsahboard/main/`) you will need to create a `secrets.h` file with the following contents:
```
#ifndef DASHBOARD_SECRETS_H
#define DASHBOARD_SECRETS_H

#endif //DASHBOARD_SECRETS_H
```


# Some Old Docs for what I did with SPIFFS
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

