#include "EnginePch.h"
#include "InputModule.h"

InputModule* InputModule::s_Instance = nullptr;
InputModule::~InputModule()
{
}

void InputModule::Init()
{
    // Ensure s_Instance is set up correctly in the actual engine code if it's not handled by the static method
    if (!s_Instance) {
        s_Instance = this;
    }

    // Initialize all states to false/zero
    for (int i = 0; i < MAX_KEY_CODES; i++)
    {
        m_Keys.Emplace(false);
    }
    for (int i = 0; i < MAX_MOUSE_BUTTONS; i++)
    {
        m_MouseButtons.Emplace(false);
    }
    
}

void InputModule::Tick(float deltaTime)
{

}

void InputModule::OnEvent(const Event& e)
{
    ProcessEvent(e);
}

void InputModule::Shutdown()
{

}


void InputModule::OnEventStatic(const Event& e)
{
    Get()->ProcessEvent(e);
}

void InputModule::ResetFrameState()
{
    Get()->ResetFrameStateInternal();
}

void InputModule::ProcessEvent(const Event& e)
{
    switch (e.Type)
    {
    case Event::Type::KeyPressed:
        if (e.Key >= 0 && e.Key < MAX_KEY_CODES)
            m_Keys[e.Key] = true;
        break;
    case Event::Type::KeyReleased:
        if (e.Key >= 0 && e.Key < MAX_KEY_CODES)
            m_Keys[e.Key] = false;
        break;
    case Event::Type::MouseButtonPressed:
        if (e.Key >= 0 && e.Key < MAX_MOUSE_BUTTONS)
            m_MouseButtons[e.Key] = true;
        break;
    case Event::Type::MouseButtonReleased:
        if (e.Key >= 0 && e.Key < MAX_MOUSE_BUTTONS)
            m_MouseButtons[e.Key] = false;
        break;
    case Event::Type::MouseMoved:
        // Calculate delta based on the difference from the previous position
        m_DeltaX = e.X - m_MouseX;
        m_DeltaY = e.Y - m_MouseY;
        
        // Update current position
        m_MouseX = e.X;
        m_MouseY = e.Y;
        break;
    case Event::Type::MouseScrolled:
        // The scroll wheel value will be reset at the end of the frame
        m_ScrollY = e.ScrollY;
        break;
    default:
        break;
    }
}

void InputModule::ResetFrameStateInternal()
{
    // Reset transient state used for frame-by-frame polling
    m_DeltaX = 0.0f;
    m_DeltaY = 0.0f;
    m_ScrollY = 0.0f;
}

// --- Static Polling Wrapper Implementations ---

bool InputModule::IsKeyPressed(uint32_t keyCode)
{
    return Get()->IsKeyPressedInternal(keyCode);
}

bool InputModule::IsMouseButtonPressed(uint32_t buttonCode)
{
    return Get()->IsMouseButtonPressedInternal(buttonCode);
}

float InputModule::GetMouseX()
{
    return Get()->GetMouseXInternal();
}

float InputModule::GetMouseY()
{
    return Get()->GetMouseYInternal();
}

InputModule::MouseDelta InputModule::GetMouseDelta()
{
    return Get()->GetMouseDeltaInternal();
}

float InputModule::GetScrollY()
{
    return Get()->GetScrollYInternal();
}

// --- Internal Non-Static Polling Implementations ---

bool InputModule::IsKeyPressedInternal(uint32_t keyCode) const
{
    if (keyCode >= MAX_KEY_CODES)
    {
        // You might want to log an error here
        return false;
    }
    return m_Keys[keyCode];
}

bool InputModule::IsMouseButtonPressedInternal(uint32_t buttonCode) const
{
    if (buttonCode >= MAX_MOUSE_BUTTONS)
    {
        // You might want to log an error here
        return false;
    }
    return m_MouseButtons[buttonCode];
}
