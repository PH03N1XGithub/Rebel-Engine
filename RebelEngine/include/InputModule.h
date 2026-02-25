#pragma once
#include "BaseModule.h"
#include "Window.h"

class InputModule : public IModule
{
	REFLECTABLE_CLASS(InputModule, IModule)
	
	InputModule() = default;
	~InputModule() override;
	
	void Init() override;
	void Tick(Float deltaTime) override;
	void OnEvent(const Event& e) override;
	void Shutdown() override;

	
    // --- Static Singleton Access ---
	static InputModule* s_Instance;
	static InputModule* Get() { return s_Instance; }

	// Max key/button codes assumed from standard GLFW/engine definitions
    static constexpr uint32_t MAX_KEY_CODES = 512;
    static constexpr uint32_t MAX_MOUSE_BUTTONS = 8;


    /**
     * @brief Static entry point for the engine's event system. Routes the event to the instance.
     * @param e The engine event to process.
     */
    static void OnEventStatic(const Event& e);

    // --- Frame State Reset ---
    /**
     * @brief Resets per-frame transient state like mouse delta and scroll.
     * IMPORTANT: Must be called once per frame, typically from BaseEngine::Tick().
     */
    static void ResetFrameState();

    // --- Mouse Delta Structure ---
    struct MouseDelta {
        Float x;
        Float y;
    };

    // --- Public Static Polling API ---

    /**
     * @brief Checks if a specific keyboard key is currently held down.
     * Note: This is now a static convenience function that uses Get().
     */
    static Bool IsKeyPressed(uint32_t keyCode);

    /**
     * @brief Checks if a specific mouse button is currently held down.
     * Note: This is now a static convenience function that uses Get().
     */
    static Bool IsMouseButtonPressed(uint32_t buttonCode);
    
    /**
     * @brief Gets the current X coordinate of the mouse.
     * Note: This is now a static convenience function that uses Get().
     */
    static Float GetMouseX();

    /**
     * @brief Gets the current Y coordinate of the mouse.
     * Note: This is now a static convenience function that uses Get().
     */
    static Float GetMouseY();
    
    /**
     * @brief Gets the mouse movement delta (change) since the last frame.
     * Note: This is now a static convenience function that uses Get().
     */
    static MouseDelta GetMouseDelta();

    /**
     * @brief Gets the last recorded vertical scroll delta.
     * Note: This is now a static convenience function that uses Get().
     */
    static Float GetScrollY();

private:
	
   	/**
   	 * @brief Internal logic to update the state based on the event.
   	 * @param e The engine event to process.
   	 */
   	void ProcessEvent(const Event& e);
   	void ResetFrameStateInternal();
	
   	// --- Internal Non-Static Polling Implementations ---
   	// These are called by the static wrappers above.
   	Bool IsKeyPressedInternal(uint32_t keyCode) const;
   	Bool IsMouseButtonPressedInternal(uint32_t buttonCode) const;
   	
   	Float GetMouseXInternal() const { return m_MouseX; }
   	Float GetMouseYInternal() const { return m_MouseY; }
   	MouseDelta GetMouseDeltaInternal() const { return {m_DeltaX, m_DeltaY}; }
   	Float GetScrollYInternal() const { return m_ScrollY; }
	
private:
	// Key and mouse button states (true = pressed)
	TArray<Bool, MAX_KEY_CODES> m_Keys;
	TArray<Bool, MAX_MOUSE_BUTTONS> m_MouseButtons;

	// Mouse position
	Float m_MouseX = 0.0f;
	Float m_MouseY = 0.0f;
        
	// Mouse delta since last frame, used for rotation
	Float m_DeltaX = 0.0f;
	Float m_DeltaY = 0.0f; 

	// Scroll delta
	Float m_ScrollY = 0.0f;
};
REFLECT_CLASS(InputModule, IModule)
END_REFLECT_CLASS(InputModule)