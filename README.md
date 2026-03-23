# ofxOceanodeHomography

Homography-based warp nodes for [ofxOceanode](https://github.com/PlaymodesStudio/ofxOceanode).

## Nodes

### Corner Pin
Applies a perspective warp to an input texture by letting you drag its four corners to arbitrary positions. An interactive floating preview window shows the image with draggable handles; the result is rendered into an output FBO at the specified Width × Height.

### ROI Warp
Lets you define an arbitrary quadrilateral region of interest on the input texture. The four corners of that region are re-mapped to fill a new output FBO at Width × Height, cropping and perspective-correcting in one step.

### Common controls (both nodes)
| Action | Effect |
|---|---|
| Drag handle | Move corner |
| Alt + drag | Fine movement (÷100) |
| Arrow keys | Nudge selected corner (0.001 step) |
| Alt + arrow | Fine nudge (0.00001 step) |
| Tab | Cycle to next corner |
| **Show** toggle | Show / hide the preview window |

The preview window locks to the image's aspect ratio when resized (floating) and works correctly both docked and floating.

## Dependencies

- [ofxOceanode](https://github.com/PlaymodesStudio/ofxOceanode)
- [ofxHomography](https://github.com/paulobarcelos/ofxHomography) (provides the homography math)

## Installation

```bash
# in your openFrameworks/addons folder:
git clone https://github.com/paulobarcelos/ofxHomography
git clone https://github.com/PlaymodesStudio/ofxOceanodeHomography
```

Add both to your project's `addons.make`:
```
ofxHomography
ofxOceanodeHomography
```

Then in your `ofApp.cpp`:
```cpp
#include "ofxOceanodeHomography.h"
// ...
ofxOceanodeHomography::registerCollection(oceanode);
```
