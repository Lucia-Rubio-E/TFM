# Indoor IoT Sensor Localization System
## Master's Thesis Project (TFM)

This repository contains the implementation of a static sensor localization system with user guidance capabilities, developed as my Master's Thesis at the University of Alcalá.

## Repository Structure

```
├── App_Unity/Assets		# Unity AR guidance application
│   ├── UIController.cs         	# User interface management
│   ├── main.cs   			# System calibration logic
│   ├── sensorHandler.cs		# Device sensor handling
│   ├── QRCodeScanner.cs		# QR code detection
│   └── ServerClient.cs			# Server communication
│
├── ESP32/			# ESP32-S3 node firmware
│   ├── anchor1/			# First anchor node
│   ├── anchor2/			# Second anchor node
│   ├── anchor3/			# Third anchor node
│   └── tag1/				# Tag node
│
├── Node-RED/			# Data flow processing
│   └── flows_node_RED.json		# Node-RED flow configuration
│
├── PostgreSQL/			# Database scripts
│   └── database_backup			# Database structure and initial data
│
└── procesamiento_nodos/	# Node processing scripts
    ├── app.py				# Flask server implementation
    ├── calcular_localizacion.py	# Location calculation
    ├── reset_tables.sql		# Database reset script
    └── resolver_trilateracion.py	# Trilateration algorithm
```

## Installation Instructions

### 1. Unity Application Setup
1. Open Unity Hub and create a new 3D project using Unity 2021.3.15f1
2. Import required packages:
   - Text Mesh Pro
   - ZXing (for QR scanning)
3. Copy the scripts from `App_Unity/Assets` to your project's scripts folder
4. Configure the build settings for Android platform
5. Update the server IP address in `ServerClient.cs`

### 2. ESP32 Node Setup
1. Install ESP-IDF v5.3.1:
```bash
git clone -b v5.3.1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh
source ./export.sh
```

2. Configure each node:
```bash
cd ESP32/anchor1  # (or anchor2/anchor3/tag1)
idf.py set-target esp32s3
idf.py menuconfig
# Enable WiFi and FTM options
```

3. Build and flash:
```bash
idf.py build
idf.py -p PORT flash monitor
```

### 3. Node-RED Configuration
1. Install Node-RED:
```bash
sudo npm install -g node-red
```

2. Install required Node-RED nodes:
```bash
# Install PostgreSQL nodes
cd ~/.node-red
npm install node-red-contrib-postgresql

# Install Mosquitto MQTT nodes
npm install node-red-contrib-mosquitto
```

3. Install and configure Mosquitto MQTT broker:
```bash
# Install Mosquitto
sudo apt-get install mosquitto mosquitto-clients

# Edit Mosquitto configuration
sudo nano /etc/mosquitto/conf.d/default.conf

# Add these lines:
listener 1884
allow_anonymous true

# Restart Mosquitto
sudo systemctl restart mosquitto
```

4. Start Node-RED:
```bash
node-red
```

5. Access http://localhost:1880

6. Configure the PostgreSQL node in Node-RED:
   - Host: localhost
   - Port: 5432
   - Database: postgres2
   - Username: postgres
   - Password: your_password

7. Configure the MQTT node in Node-RED:
   - Server: localhost
   - Port: 1884
   - Topic: data

8. Import the flow from `Node-RED/flows_node_RED.json`

### 4. PostgreSQL Database Setup
1. Install PostgreSQL:
```bash
sudo apt install postgresql postgresql-contrib
```

2. Create database and restore backup:
```bash
sudo -u postgres createdb postgres2
sudo -u postgres psql postgres2 < PostgreSQL/database_backup
```

3. Resetting Database Tables (for testing):
```bash
# To clear all measurement data and reset sequences:
sudo -u postgres psql postgres2 < procesamiento_nodos/reset_tables.sql
```

This reset script will delete all entries from data_tag table, delete all entries from devices table, and reset the ID sequences for both tables.


### 5. Processing Scripts Setup
1. Create Python virtual environment:
```bash
python -m venv venv
source venv/bin/activate
```

2. Install dependencies:
```bash
pip install flask psycopg2 numpy pandas
```

3. Start the location calculation script in a terminal:
```bash
cd procesamiento_nodos
python calcular_localizacion.py
```
This script will connect to PostgreSQL database, process distance measurements, calculate node positions and update node positions in the database (it runs with 3-second update intervals).

4. Start Flask server in another terminal:
```bash
cd procesamiento_nodos
python app.py
```

Note: Both scripts need to be running simultaneously. The location calculation script processes the raw measurements and updates positions, while the Flask server provides the REST API for querying these positions.


## Configuration

1. ESP32 Nodes
    - Configure WiFi settings: SSID and password.
    - Configure FTM parameters if necessary (adjust based on the environment).
2. Unity Application
    - Update the server IP (the REST API URL) in ServerClient.cs.

## Usage

1. Position and power up the nodes
2. Start the Flask server and Node-RED
3. Run the Unity application on Android device
4. Follow the calibration process:
   - Scan Anchor 1 QR code
   - Scan Anchor 2 QR code
   - Wait for system calibration
5. Select target node for guidance

## Demo Video

A complete demonstration of the system can be found [here](https://youtu.be/Hp4ShoctpZ8).
