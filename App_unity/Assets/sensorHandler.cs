using UnityEngine;
using UnityEngine.UI;
using System.Collections;
using System.Collections.Generic;
using TMPro;

public class GyroscopeAndCompassHandler : MonoBehaviour
{
    [SerializeField] private TextMeshProUGUI sensorDataText;

    private float[] accelerometerReading = new float[3];
    private float[] magnetometerReading = new float[3];
    private float[] orientationAngles = new float[3];
    private float geomagneticHeading;
    private Vector3 cartesianCoordinates;

    private bool isRecording = false;
    private List<float> magnetometerData = new List<float>();

    private void Start()
    {
        if (sensorDataText == null)
        {
            Debug.LogError("SensorDataText is not assigned in the Inspector.");
            return;
        }
        EnableSensors();
    }

    private void Update()
    {
        UpdateSensorData();
        sensorDataText.text = $"Orientation Angles : {orientationAngles[0]:F2}°\n";
    }

    public void EnableSensors()
    {
        if (!SystemInfo.supportsGyroscope)
        {
            Debug.LogError("Device does not support gyroscope/compass.");
            return;
        }

        if (!Input.compass.enabled)
        {
            Input.compass.enabled = true;
            Debug.Log("Compass enabled.");
        }
    }

    public void StartRecording()
    {
        float recordingInterval = 0.1f;
        if (!isRecording)
        {
            isRecording = true;
            magnetometerData.Clear();
            StartCoroutine(RecordMagnetometerData(recordingInterval));
            Debug.Log("Started recording magnetometer data.");
        }
        else
        {
            Debug.LogWarning("Recording already in progress.");
        }
    }

    public void StopRecording()
    {
        if (!isRecording)
        {
            Debug.LogWarning("StopRecording called but no recording is in progress.");
            return;
        }
        isRecording = false;
        StopAllCoroutines();
        Debug.Log("Stopped recording magnetometer data.");
    }

    public float GetAverageHeading()
    {
        if (magnetometerData.Count == 0)
        {
            Debug.LogWarning("No magnetometer data recorded.");
            return 0;
        }

        int totalCount = magnetometerData.Count;
        int startIndex = Mathf.FloorToInt(totalCount * 0.2f); // Inicio después del 20%
        int endIndex = Mathf.CeilToInt(totalCount * 0.8f);    // Fin antes del 80%

        if (endIndex <= startIndex)
        {
            Debug.LogWarning("Not enough data to calculate average after filtering.");
            return 0;
        }

        float sum = 0;
        int validCount = 0;

        for (int i = startIndex; i < endIndex; i++)
        {
            sum += magnetometerData[i];
            validCount++;
        }

        return validCount > 0 ? sum / validCount : 0;
    }


    public float GetCurrentHeading()
    {
        float heading = Input.compass.magneticHeading;
        Debug.Log($"Current heading: {heading}°\n");
        return heading;
    }

    private void UpdateSensorData()
    {
        accelerometerReading[0] = Input.acceleration.x;
        accelerometerReading[1] = Input.acceleration.y;
        accelerometerReading[2] = Input.acceleration.z;

        magnetometerReading[0] = Input.compass.rawVector.x;
        magnetometerReading[1] = Input.compass.rawVector.y;
        magnetometerReading[2] = Input.compass.rawVector.z;

        geomagneticHeading = Input.compass.magneticHeading;

        orientationAngles[0] = geomagneticHeading; // Azimuth (Heading)
        orientationAngles[1] = Mathf.Atan2(accelerometerReading[1], accelerometerReading[2]) * Mathf.Rad2Deg; // Pitch
        orientationAngles[2] = Mathf.Atan2(accelerometerReading[0], accelerometerReading[2]) * Mathf.Rad2Deg; // Roll
    }

    private IEnumerator RecordMagnetometerData(float interval)
    {
        while (isRecording)
        {
            float heading = Input.compass.magneticHeading;
            magnetometerData.Add(heading);
            Debug.Log($"Recorded heading: {heading}°\n");
            yield return new WaitForSeconds(interval);
        }
    }
}
