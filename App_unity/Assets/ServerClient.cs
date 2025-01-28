using System.Collections;
using UnityEngine;
using UnityEngine.Networking;
using System.Threading.Tasks;

public class ServerClient : MonoBehaviour
{
    [SerializeField] private string publicServerIP;

    public IEnumerator GetDevicePosition(string macAddress, string anchorId, System.Action<string, string> onSuccess, System.Action<string> onError)
    {
        string url = $"{publicServerIP}/device_position?mac={macAddress}";
        Debug.Log($"Requesting server at: {url}");

        using (UnityWebRequest webRequest = UnityWebRequest.Get(url))
        {
            yield return webRequest.SendWebRequest();

            if (webRequest.result == UnityWebRequest.Result.Success)
            {
                onSuccess?.Invoke(anchorId, webRequest.downloadHandler.text);
            }
            else
            {
                onError?.Invoke(webRequest.error);
            }
        }
    }
}





