//
//  ofxOceanodeHomography.h
//  ofxOceanodeHomography
//
//  Umbrella header — include this in your ofApp and call
//  ofxOceanodeHomography::registerCollection(oceanode).
//

#pragma once

#include "cornerPin.h"
#include "ROI_Warp.h"

namespace ofxOceanodeHomography {
    static void registerCollection(ofxOceanode& oceanode) {
        oceanode.registerModel<cornerPin>();
        oceanode.registerModel<ROI_Warp>();
    }
}
