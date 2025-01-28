using UnityEngine;
using UnityEngine.UI;
using TMPro;

public class UIController : MonoBehaviour
{

    [SerializeField] private Button buttonAnchor1;
    [SerializeField] private Button buttonAnchor2;
    [SerializeField] private Button buttonAnchor3;
    [SerializeField] private Button buttonTag1;
    [SerializeField] private Button buttonTag2;

    [SerializeField] private TextMeshProUGUI statusText;
    [SerializeField] private TextMeshProUGUI serverResponseText;
    [SerializeField] private TextMeshProUGUI textPhase;
    [SerializeField] private RectTransform arrowImage;

    public void UpdateStatus(string message)
    {
        if (statusText != null) statusText.text = message;
        Debug.Log("Status updated: " + message);
    }

    public void UpdateServerResponse(string message)
    {
        if (serverResponseText != null) serverResponseText.text = message;
        Debug.Log("Server response: " + message);
    }

    public void UpdatePhaseText(string currentPhase) 
    {
        if (textPhase != null)
        {
            textPhase.text = $"Current Phase: {currentPhase}";
        }
    }

    public void SetupButton(string buttonId, System.Action onClickAction)
    {
        Button targetButton = GetButtonById(buttonId);
        if (targetButton != null)
        {
            targetButton.onClick.AddListener(() => onClickAction?.Invoke());
        }
    }

    public void SetButtonState(string buttonId, bool interactable)
    {
        Button targetButton = GetButtonById(buttonId);
        if (targetButton != null)
        {
            targetButton.interactable = interactable;
        }
    }

    public void DisableAllButtons()
    {
        buttonAnchor1.interactable = false;
        buttonAnchor2.interactable = false;
        buttonAnchor3.interactable = false;
        buttonTag1.interactable = false;
        buttonTag2.interactable = false;
    }

    public void EnableAllButtons()
    {
        buttonAnchor1.interactable = true;
        buttonAnchor2.interactable = true;
        buttonAnchor3.interactable = true;
        buttonTag1.interactable = true;
        buttonTag2.interactable = true;
    }

    public void UpdateCompass(float heading)
    {
        if (arrowImage != null)
        {
            arrowImage.localRotation = Quaternion.Euler(0, 0, -heading);
        }
    }

    private Button GetButtonById(string buttonId)
    {
        switch (buttonId)
        {
            case "anchor1": return buttonAnchor1;
            case "anchor2": return buttonAnchor2;
            case "anchor3": return buttonAnchor3;
            case "tag1": return buttonTag1;
            case "tag2": return buttonTag2;
            default: return null;
        }
    }

    public void ShowArrow()
    {
        if (arrowImage != null)
        {
            arrowImage.gameObject.SetActive(true);
            Debug.Log("Arrow image shown.");
        }
    }

    public void HideArrow()
    {
        if (arrowImage != null)
        {
            arrowImage.gameObject.SetActive(false);
            Debug.Log("Arrow image hidden.");
        }
    }

}
