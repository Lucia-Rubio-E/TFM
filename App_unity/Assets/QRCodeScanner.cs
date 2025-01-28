using UnityEngine;
using UnityEngine.UI;
using ZXing; // QR code reader library
using TMPro;

public class QRCodeScanner : MonoBehaviour
{
    private WebCamTexture webcamTexture;
    private IBarcodeReader barcodeReader;
    public RawImage cameraFeed;
    public TextMeshProUGUI qrResultText;

    void Start()
    {
        barcodeReader = new BarcodeReader();
        webcamTexture = new WebCamTexture();

        if (cameraFeed != null)
        {
            cameraFeed.texture = webcamTexture;
        }

        if (webcamTexture != null)
        {
            webcamTexture.Play();
            Debug.Log("Webcam started successfully.");
        }
        else
        {
            Debug.LogError("Failed to initialize the webcam.");
        }
    }

    void Update()
    {
        if (webcamTexture != null && webcamTexture.isPlaying)
        {
            try
            {
                var frame = webcamTexture.GetPixels32();
                var result = barcodeReader.Decode(frame, webcamTexture.width, webcamTexture.height);

                if (result != null)
                {
                    if (qrResultText.text != result.Text)
                    {
                        qrResultText.text = result.Text;
                        Debug.Log($"QR Code detected: {result.Text}");

                        FindObjectOfType<AnchorCalibration>()?.HandleQRDetection(result.Text);
                    }
                }
            }
            catch (System.Exception ex)
            {
                Debug.LogError($"Error reading QR Code: {ex.Message}");
            }
        }
        else
        {
            Debug.LogWarning("Webcam is not active or is not capturing frames.");
        }
    }

}
