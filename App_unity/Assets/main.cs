using UnityEngine;
using UnityEngine.UI;
using System;
using System.Collections;
using System.Collections.Generic;
using TMPro;

public class AnchorCalibration : MonoBehaviour
{
    [SerializeField] private QRCodeScanner qrScanner;
    [SerializeField] private GyroscopeAndCompassHandler sensorHandler;
    [SerializeField] private UIController uiController;
    [SerializeField] private ServerClient serverClient;

    [SerializeField] private string macAnchor1 = "00:11:22:33:44:01";
    [SerializeField] private string macAnchor2 = "00:11:22:33:44:02";
    [SerializeField] private string macAnchor3 = "00:11:22:33:44:03";
    [SerializeField] private string macTag1 = "00:11:22:33:44:04";
    [SerializeField] private string macTag2 = "00:11:22:33:44:05";

    [System.Serializable] private class PositionResponse
    {
        public float positionx;
        public float positiony;
    }   

    private readonly Dictionary<string, (float x, float y)> nodePositions = new Dictionary<string, (float x, float y)>();
    private CalibrationFlowManager flowManager;
    private Coroutine orientationCoroutine;

    private string currentNodeId = string.Empty;
    private string targetNodeId = string.Empty;

    void Start()
    {
        Debug.Log("AnchorCalibration script initialized.");

        ValidateDependencies();

        uiController.SetupButton("anchor1", () => HandleButtonClicked("anchor1"));
        uiController.SetupButton("anchor2", () => HandleButtonClicked("anchor2"));
        uiController.SetupButton("anchor3", () => HandleButtonClicked("anchor3"));
        uiController.SetupButton("tag1", () => HandleButtonClicked("tag1"));
        uiController.SetupButton("tag2", () => HandleButtonClicked("tag2"));
        uiController.HideArrow();

        uiController.DisableAllButtons();
        sensorHandler.EnableSensors();

        flowManager = new CalibrationFlowManager();
        flowManager.OnPhaseChanged += OnPhaseChanged;

        flowManager.SetPhase(CalibrationFlowManager.CalibrationPhase.WaitingQRAnchor1);
        uiController.UpdateStatus("Starting calibration. Go to Anchor 1.");
        uiController.UpdatePhaseText(flowManager.CurrentPhase.ToString());

    }

    private void ValidateDependencies()
    {
        if (sensorHandler == null || qrScanner == null || uiController == null || serverClient == null)
        {
            Debug.LogError("Missing references in AnchorCalibration.");
            enabled = false;
            return;
        }
    }

    public void HandleQRDetection(string anchorId)
    {
        if (anchorId == "anchor1" && flowManager.CurrentPhase == CalibrationFlowManager.CalibrationPhase.WaitingQRAnchor1)
        {
            uiController.UpdateStatus("Anchor 1 QR detected. Please press the button.");
            uiController.SetButtonState("anchor1", true);
            flowManager.SetPhase(CalibrationFlowManager.CalibrationPhase.WaitingButtonAnchor1);
        }
        else if (anchorId == "anchor2" && flowManager.CurrentPhase == CalibrationFlowManager.CalibrationPhase.WaitingQRAnchor2)
        {
            uiController.UpdateStatus("Anchor 2 QR detected. Please press the button.");
            uiController.SetButtonState("anchor2", true);
            flowManager.SetPhase(CalibrationFlowManager.CalibrationPhase.WaitingButtonAnchor2);
        }
        else if (anchorId == targetNodeId && flowManager.CurrentPhase == CalibrationFlowManager.CalibrationPhase.OrientationoftheUser)
        {
            uiController.UpdateStatus("Goal QR detected. Please press the button."); ///// GuideUserToTarget
            currentNodeId=anchorId;
            flowManager.SetPhase(CalibrationFlowManager.CalibrationPhase.WaitingSomeButton );
        }
        else
        {
            Debug.LogWarning($"Unrecognized or invalid QR Code: {anchorId}");
        }
        uiController.UpdatePhaseText(flowManager.CurrentPhase.ToString());
    }

    private void HandleButtonClicked(string deviceId)
    {
        if (deviceId == "anchor1" && flowManager.CurrentPhase == CalibrationFlowManager.CalibrationPhase.WaitingButtonAnchor1)
        {
            uiController.UpdateStatus($"Button for {deviceId} pressed.");
            flowManager.SetPhase(CalibrationFlowManager.CalibrationPhase.WaitingServerResponseAnchor1);
        }
        else if (deviceId == "anchor2" && flowManager.CurrentPhase == CalibrationFlowManager.CalibrationPhase.WaitingButtonAnchor2)
        {
            uiController.UpdateStatus($"Button for {deviceId} pressed.");
            flowManager.SetPhase(CalibrationFlowManager.CalibrationPhase.WaitingServerResponseAnchor2);
        }
        else if (flowManager.CurrentPhase == CalibrationFlowManager.CalibrationPhase.WaitingSomeButton)
        {
            targetNodeId = deviceId;
            flowManager.SetPhase(CalibrationFlowManager.CalibrationPhase.OrientationoftheUser);
            Debug.Log("Transitioning to OrientationoftheUser phase.");
        }
        else
        {
            uiController.UpdateStatus($"Invalid button click for {deviceId} in current phase.");
        }
        uiController.UpdatePhaseText(flowManager.CurrentPhase.ToString());
    }

    private void OnServerResponseSuccess(string deviceId, string responseText)
    {
        uiController.UpdateServerResponse(responseText);

        if (deviceId == "anchor1" && flowManager.CurrentPhase == CalibrationFlowManager.CalibrationPhase.WaitingServerResponseAnchor1)
        {
            flowManager.SetPhase(CalibrationFlowManager.CalibrationPhase.WaitingQRAnchor2);
            uiController.UpdateStatus("Successful connection to Anchor 1. Go to Anchor 2.");
            Debug.Log($"Successful connection to Anchor 1. Go to Anchor 2");
            StartCalibration();
        }
        else if (deviceId == "anchor2" && flowManager.CurrentPhase == CalibrationFlowManager.CalibrationPhase.WaitingServerResponseAnchor2)
        {
            flowManager.SetPhase(CalibrationFlowManager.CalibrationPhase.CalibrationComplete);
            uiController.UpdateStatus("Calibration complete.");
            flowManager.SetPhase(CalibrationFlowManager.CalibrationPhase.WaitingSomeButton);
            Debug.Log($"Calibration complete");
            StopCalibration();
        }
        uiController.UpdatePhaseText(flowManager.CurrentPhase.ToString());
    }

    private void OnServerResponseError(string error)
    {
        uiController.UpdateServerResponse("Error connecting to the server: " + error);
        uiController.UpdateStatus("Error connecting to the server.");
        flowManager.SetPhase(CalibrationFlowManager.CalibrationPhase.ErrorServer);
        uiController.UpdatePhaseText(flowManager.CurrentPhase.ToString());
        Debug.Log($"Error connecting to the server.");
    }

    private void StartCalibration()
    {
        Debug.Log("Starting calibration...");
        sensorHandler.StartRecording();
    }

    public void StopCalibration()
    {
        Debug.Log("Stopping calibration...");
        sensorHandler.StopRecording();
        float averageHeading = sensorHandler.GetAverageHeading();
        uiController.UpdateStatus($"Calibration complete. Average heading: {averageHeading}°");
    }

    private void OnPhaseChanged(CalibrationFlowManager.CalibrationPhase newPhase)
    {
        Debug.Log($"Transitioning to phase: {newPhase}");

        switch (newPhase)
        {
            case CalibrationFlowManager.CalibrationPhase.WaitingServerResponseAnchor1:
                StartServerCall(macAnchor1, "anchor1", CalibrationFlowManager.CalibrationPhase.WaitingQRAnchor2);
                break;

            case CalibrationFlowManager.CalibrationPhase.WaitingServerResponseAnchor2:
                StartServerCall(macAnchor2, "anchor2", CalibrationFlowManager.CalibrationPhase.CalibrationComplete);
                break;

            case CalibrationFlowManager.CalibrationPhase.CalibrationComplete:
                StopCalibration();
                StartServerCall(macAnchor3, "anchor3", CalibrationFlowManager.CalibrationPhase.CalibrationComplete, () =>
                    StartServerCall(macTag1, "tag1", CalibrationFlowManager.CalibrationPhase.CalibrationComplete, () =>
                        StartServerCall(macTag2, "tag2", CalibrationFlowManager.CalibrationPhase.WaitingSomeButton)));
                currentNodeId="anchor2";
                break;

            case CalibrationFlowManager.CalibrationPhase.WaitingSomeButton:
                uiController.EnableAllButtons(); 
                uiController.UpdateStatus("Please press any button.");
                break;

            case CalibrationFlowManager.CalibrationPhase.OrientationoftheUser:
                uiController.UpdateStatus("Align yourself with the target using the compass.");
                Debug.Log("User orientation phase started.");
                uiController.ShowArrow();
                StartUserOrientation(currentNodeId,targetNodeId); 
                break; 

        }

        uiController.UpdatePhaseText(flowManager.CurrentPhase.ToString());
    }

    private void StartServerCall(string mac, string nodeId, CalibrationFlowManager.CalibrationPhase nextPhase, Action onComplete = null)
    {
        StartCoroutine(serverClient.GetDevicePosition(mac, nodeId, (deviceId, responseText) =>
        {
            try
            {
                var positionData = JsonUtility.FromJson<PositionResponse>(responseText);
                nodePositions[deviceId] = (positionData.positionx, positionData.positiony);
                Debug.Log($"Position for {deviceId}: X={positionData.positionx}, Y={positionData.positiony}");
            }
            catch (Exception ex)
            {
                Debug.LogError($"Failed to parse position for {deviceId}: {ex.Message}");
            }

            OnServerResponseSuccess(deviceId, responseText);

            onComplete?.Invoke();
        }, OnServerResponseError));
    }

    private void StartUserOrientation(string currentNodeId, string targetNodeId)
    {
        if (orientationCoroutine != null)
        {
            StopCoroutine(orientationCoroutine);
        }
        orientationCoroutine = StartCoroutine(GuideUserToTarget(currentNodeId,targetNodeId));
    }

    private IEnumerator GuideUserToTarget(string currentNodeId, string targetNodeId)
    {
        while (flowManager.CurrentPhase == CalibrationFlowManager.CalibrationPhase.OrientationoftheUser)
        {
            if (!nodePositions.ContainsKey(currentNodeId) || !nodePositions.ContainsKey(targetNodeId))
            {
                Debug.LogError("Missing position data for target node or anchor2.");
                yield break;
            }

            var (xTarget, yTarget) = nodePositions[targetNodeId];
            var (xCurrent, yCurrent) = nodePositions[currentNodeId];

            float targetAngle = Mathf.Atan2(yTarget - yCurrent, xTarget - xCurrent) * Mathf.Rad2Deg; // 135º  ( en el papel)
            float systemOrientation = sensorHandler.GetAverageHeading(); //      (en la realidad)
            float currentHeading = sensorHandler.GetCurrentHeading(); // actual user heading

            // ángulo del objetivo según la orientación del sistema
            float realTargetAngle = -targetAngle + systemOrientation;
            float angleOfRotation = Mathf.DeltaAngle(currentHeading, realTargetAngle);


            uiController.UpdateCompass(angleOfRotation); // Actualiza la brújula
            uiController.UpdateStatus($"Aligning to Target:\nReal target Angle: {realTargetAngle}°\nSystem Orientation: {systemOrientation}°\n" +
                                    $"User Heading: {currentHeading}°\nRotation Needed: {angleOfRotation}°\n" +
                                    $"Current: {currentNodeId}, Target: {targetNodeId} ");

            yield return new WaitForSeconds(0.1f);
        }
        Debug.Log("Orientation phase complete.");
    }

    public class CalibrationFlowManager
    {
        public enum CalibrationPhase
        {
            WaitingQRAnchor1,
            WaitingButtonAnchor1,
            WaitingServerResponseAnchor1,
            WaitingQRAnchor2,
            WaitingButtonAnchor2,
            WaitingServerResponseAnchor2,
            ErrorServer,
            CalibrationComplete,
            WaitingSomeButton,
            OrientationoftheUser
        }

        public CalibrationPhase CurrentPhase { get; private set; }
        public delegate void PhaseChanged(CalibrationPhase newPhase);
        public event PhaseChanged OnPhaseChanged;

        public void SetPhase(CalibrationPhase newPhase)
        {
            CurrentPhase = newPhase;
            Debug.Log("Phase updated to: " + CurrentPhase);
            OnPhaseChanged?.Invoke(CurrentPhase);
        }

    }
}
