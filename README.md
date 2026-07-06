# Group 25 – Neura Personal Assistant

Neura is compatible with Unix-based systems like Mac-OS.

## Setup (First Time)

Run this one command to install all dependencies:
```bash
make setup
```

Then compile and run:
```bash
make clean
make
./mainAssistant
```

---

## Returning to the Project

Open a new terminal session, recompile and run:
```bash
make clean
make
./mainAssistant
```

---

## Platform-Specific Setup Notes

### macOS
- Uses Homebrew for dependencies (opencv, sfml, portaudio, etc.)
- Metal acceleration is enabled for Whisper
- Requires system permissions for camera and microphone

**Camera Issue (Important)** 
macOS may default your camera to your iPhone (Continuity Camera) instead of your built-in webcam.

If vision features are not working:
1. Open a camera-based app (Photo Booth or FaceTime)
2. Check which camera is selected
3. On laptop, in System Settings switch from iPhone camera to built-in webcam
4. Restart `mainAssistant`

### Linux (WSL)

Note: Neura works flawlessly with MacOS. However, if you are using Neura on Linux/Windows through WSL, certain commands may not work as WSL cannot access certain system features (eg. hardware brightness, Windows applications, OpenCV Camera) without significant work in your powershell. Windows/WSL functionality has not been fully integrated and is meant as a bonus addition to the base requirements of the project. If you are curious and would like to see how some of the features work in WSL, you must at least do the following:

Microphone:
- In your WSL, use the following command to check your microphone status (look for RDPSource)
```
pactl list sources short
```
- Ensure that RDPSource appears

---

## Required Permissions (All Systems)

If camera, microphone, or vision features fail, verify:

- Camera access is enabled
- Microphone access is enabled
- Terminal or IDE has permission to access devices

### macOS:
System Settings → Privacy & Security → Camera / Microphone

### Linux:
Depends on distribution (PulseAudio or PipeWire configuration)

---

## API Token Setup for Google Calendar

1. If you have completed the API Token Setup for Google Calendar, skip to step 4. Otherwise, go to https://console.cloud.google.com and sign in using your desired account
2. Click on the Project Picker ("Select a Project") button at the top of the screen
3. Click "New Project" button, name the project whatever you'd like ("eg. Voice Assistant) and ignore the parent resource box. Click "Create".
4. Using the Project Picker button, select the project you have just created/want to use. Go to https://console.cloud.google.com/apis/library/calendar-json.googleapis.com and click the "Enable" button. Then, click on the "APIs & Services" tab from the quick select menu or the sidebar of the left side of the screen.
5. On the APIs & Services page, click on the "Credentials" tab on the left sidebar. CLick the "Create credentials" button and select "OAuth client ID." Click the "Configure consent screen button. 
6. You will be redirected to the Branding page. Click the "Get Started" button. For step one, enter a name for the virtual assistant (You can name it whatever you wish) and your email as the user support email. Select "External" for the second "Audience" step. Input your own email again for the third Contact Information step. Click the "Agree" button in the final finishing step to agree to the User Data Policy Google API has, and click create to finish.
7. Click on the Audience tab in the left sidebar. Scroll down to Test users and click Add users. Input your email in the textbox and click Save.
8. Click on the Overview tab in the left sidebar. You'll be returned to the OAuth Overview page. Click "Create OAuth client." 
9. On the "Create OAuth client ID" page, select Web application for the Application type and give it any name you want. 
10. Scroll down to the "Authorized redirect URIs" section and click on the "Add URI" button. Paste in https://developers.google.com/oauthplayground and hit the blue Create button at the bottom of the screen.
11. On the OAuth client creation popup, you will see two important credentials, the Client ID and Client Secret. Copy and save these two credentials somwhere safe, or click the Download JSON button at the bottom to save a file containing these two credentials, as you will not be able to access the Client Secret once the popup is closed.
12. Go to the sidebar and hover over APIs & Services. Then select OAuth consent screen.
13. Go to https://developers.google.com/oauthplayground and log in. Click on the settings (gear button) at the top right. Check the "Use your own Oauth credentials" checkbox. Then, paste in the Client ID and Client Secret you obtained in the previous step.
14. On the left half of the page, search for "Google Calendar API v3" in the "Step 1 Select & authorize APIs" section. Click the dropdown and check off the "https://www.googleapis.com/auth/calendar.events" scope. Then, click on the blue Authorize APIs button.
15. Click Exchange authorization code for tokens and copy the Refresh token value from the "Step 2 Exchange authorization code for tokens" section. 
16. Open Neura and navigate to the settings page. Paste in your saved Client ID, Client Secret, and the Refresh Token obtained in the last step. Click the save button. Now, you are all set up to add calendar events to your Google Calendar with the help of Neura!

NOTE: Refresh tokens will eventually expire. For example, if your Google Cloud project is in the "Testing" publishing status, refresh tokens expire in 7 days. If the token is inactive for over 6 months, it may also expire. Ensure that you get a new refresh token using your credentials by following steps 11 to 14 once it expires!

---

## API Token Setup for YouTube
1. If you have completed the API Token Setup for Google Calendar, go to https://console.cloud.google.com and skip to step 4. 
2. Click on the Project Picker ("Select a Project") button at the top of the screen
3. Click "New Project" button, name the project whatever you'd like ("eg. Voice Assistant) and ignore the parent resource box. Click "Create".
4. Using the Project Picker button, select the project you have just created/want to use. Go to https://console.cloud.google.com/apis/library/youtube.googleapis.com and click the "Enable" button. Then, click on the "APIs & Services" tab from the quick select menu or the sidebar of the left side of the screen.
5. On the APIs & Services page, click on the "Credentials" tab on the left sidebar. Click the "Create credentials" button and select "API key." 
6. Name your API Key (eg. YouTube API Key) and select YouTube Data API v3 from the "APIs that can be acessed using this key" dropdown. Then, scroll down and click the blue create button
7. Open Neura and navigate to the settings page. Copy the created API key and paste it in to the appropriate textbook. Now you're all set up!

---

## Gesture Actions

- 1 finger (👆) : Move cursor
- 2 fingers (✌️) : Click and Drag
  - For a single click: Show 2 fingers briefly, then switch to another gesture
  - For click and drag: Hold 2 fingers and move your hand
- Open Palm (🖐️): Cancels Neura speaking and other gestures
- Thumbs Down (👎): No
- Thumbs Up (👍): Yes

## Voice Actions

Please see in-app tutorial or in-app Quick Reference Sheet (Question mark button).

## Extra Notes

Updated UML Diagram Here: https://gitlab.sci.uwo.ca/courses/2026/01/COMPSCI3307/group25/-/wikis/Stage-4-Final-Project-Submission