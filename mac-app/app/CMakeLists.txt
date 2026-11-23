# ET Broadcaster Studio — JUCE app. Everything Mac-specific lives in Source/platform/*_mac.mm;
# the engine (../engine) and all other sources are portable (Linux builds today, Windows later).
juce_add_gui_app(ETBStudio
    PRODUCT_NAME "ET Broadcaster"
    COMPANY_NAME "E.T.TATTOO"
    BUNDLE_ID com.lowrideliferadio.etbroadcaster
    VERSION ${PROJECT_VERSION}
    ICON_BIG ${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon-1024.png
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "ET Broadcaster uses the microphone for live talk-overs when you GO LIVE."
    NEEDS_CURL TRUE
    HARDENED_RUNTIME_ENABLED FALSE)

juce_add_binary_data(ETBStudioData SOURCES Resources/logo-256.png Resources/lockup.png)

target_sources(ETBStudio PRIVATE
    Source/Main.cpp Source/MainComponent.cpp Source/StudioLink.cpp Source/DeckView.cpp Source/IcecastSource.cpp)
if(APPLE)
    target_sources(ETBStudio PRIVATE Source/platform/SecretStore_mac.mm)
    target_link_libraries(ETBStudio PRIVATE "-framework Security")
else()
    target_sources(ETBStudio PRIVATE Source/platform/SecretStore_generic.cpp)   # Windows: replace with Credential Manager (see ROADMAP)
endif()

target_compile_definitions(ETBStudio PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:ETBStudio,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:ETBStudio,JUCE_VERSION>")

target_link_libraries(ETBStudio PRIVATE etb_engine ETBStudioData
    juce::juce_audio_utils juce::juce_audio_devices juce::juce_audio_formats juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
