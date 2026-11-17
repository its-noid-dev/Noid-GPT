# NoidGPT 1.2.3

- Added an animated progress bar while Open WebUI is downloaded, installed, and started.
- Uses an indeterminate animation because the package installer does not expose a reliable total download size; no fake percentage is shown.
- Stops the animation when startup succeeds or fails.
