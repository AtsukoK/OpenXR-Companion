# ControllerSilhouettes

Traces the controller pictures from real 3D models: the WebXR input profile models
(https://github.com/immersive-web/webxr-input-profiles, `packages/assets/profiles`, MIT licence, (c) 2019 Amazon).
Only outlines are kept (no textures, no logos).

1. Download `left.glb` and `right.glb` of each profile in `Program.cs` (`Profiles`) into `<models>/<profile>/`.
2. `dotnet run -c Release -- <models> <repo root>`
3. It writes `editor/ControllerSilhouettes.g.cs`, `layer/src/controller_silhouettes.inc` and pictures for checking
   in `preview/` (orange = estimated: parts that are part of the body in the model, like the thumbrest).

The top view looks at the buttons with the handle down (WebXR grip space: the handle runs along Z); the side view
keeps the handle upright with the trigger at the front. `SIL_AXES=<profile>` renders the six straight-on views of
one model, to pick a turn for controllers like the Index (`topTurnDeg`).
The models themselves are not kept in the repository.
