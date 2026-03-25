//
//  ROIWarpGuiDisplay.h
//  ofxOceanode
//
//  ROI warp node rendered entirely inside the node inspector.
//  Input and output textures are selected via portal dropdowns
//  (same pattern as textureDisplay / slider). No canvas port pins.
//

#ifndef ROIWarpGuiDisplay_h
#define ROIWarpGuiDisplay_h

#include "ofMain.h"
#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeInspectorController.h"
#include "portal.h"
#include "ofxHomography.h"

class ROIWarpGuiDisplay : public ofxOceanodeNodeModel {
public:
    ROIWarpGuiDisplay() : ofxOceanodeNodeModel("ROI Warp Display") {
        selectedInputPortal  = nullptr;
        selectedOutputPortal = nullptr;
        needsDelayedRestore  = false;
    }

    void setup() override {
        setFlags(ofxOceanodeNodeModelFlags_TransparentNode);

        // ---- Output size ----
        addInspectorParameter(outputWidth.set("Width",  1920, 1, INT_MAX));
        addInspectorParameter(outputHeight.set("Height", 1080, 1, INT_MAX));

        // ---- Input portal dropdown ----
        addInspectorParameter(globalSearchIn.set("Global Search In", false));
        addInspectorParameter(selectedInputPortalName.set("Input Portal", ""));
        updatePortalListOnly(inputPortalNames, inputCompatiblePortals, globalSearchIn.get());
        ofxOceanodeInspectorController::registerInspectorDropdown(
            "ROI Warp Display", "Input Portal", inputPortalNames);
        selectedInputPortalIndex.set("Input Portal", 0, 0,
            std::max(0, (int)inputPortalNames.size() - 1));
        addInspectorParameter(selectedInputPortalIndex);

        // ---- Output portal dropdown ----
        addInspectorParameter(globalSearchOut.set("Global Search Out", false));
        addInspectorParameter(selectedOutputPortalName.set("Output Portal", ""));
        updatePortalListOnly(outputPortalNames, outputCompatiblePortals, globalSearchOut.get());
        ofxOceanodeInspectorController::registerInspectorDropdown(
            "ROI Warp Display", "Output Portal", outputPortalNames);
        selectedOutputPortalIndex.set("Output Portal", 0, 0,
            std::max(0, (int)outputPortalNames.size() - 1));
        addInspectorParameter(selectedOutputPortalIndex);

        // ---- Preview width (inspector canvas size) ----
        addInspectorParameter(previewWidth.set("Preview Width", 400.f, 50.f, 1920.f));

        // ---- Warp state ----
        inputTexWidth  = 0;
        inputTexHeight = 0;
        meshUsePixelCoords = false;
        pointDraggingIndex = -1;
        warpPoints[0] = glm::vec2(0, 0);
        warpPoints[1] = glm::vec2(1, 0);
        warpPoints[2] = glm::vec2(1, 1);
        warpPoints[3] = glm::vec2(0, 1);

        // ---- Canvas custom region ----
        addCustomRegion(displayRegion.set("Display", [this](){ drawDisplay(); }),
                        [this](){ drawDisplay(); });

        // ---- Listeners ----
        dropdownInListener = selectedInputPortalIndex.newListener([this](int &) {
            if(!ofxOceanodeShared::isPresetLoading()) updateSelectedInputPortal();
        });
        dropdownOutListener = selectedOutputPortalIndex.newListener([this](int &) {
            if(!ofxOceanodeShared::isPresetLoading()) updateSelectedOutputPortal();
        });
        globalSearchInListener = globalSearchIn.newListener([this](bool &) {
            updatePortalList(inputPortalNames, inputCompatiblePortals, globalSearchIn.get(),
                             "ROI Warp Display", "Input Portal",
                             selectedInputPortalIndex, selectedInputPortal);
        });
        globalSearchOutListener = globalSearchOut.newListener([this](bool &) {
            updatePortalList(outputPortalNames, outputCompatiblePortals, globalSearchOut.get(),
                             "ROI Warp Display", "Output Portal",
                             selectedOutputPortalIndex, selectedOutputPortal);
        });
        presetLoadedListener = ofxOceanodeShared::getPresetHasLoadedEvent().newListener([this]() {
            updatePortalListOnly(inputPortalNames, inputCompatiblePortals, globalSearchIn.get());
            restoreSelectionByName(inputPortalNames, inputCompatiblePortals,
                                   selectedInputPortalIndex, selectedInputPortal,
                                   selectedInputPortalName.get());
            updatePortalListOnly(outputPortalNames, outputCompatiblePortals, globalSearchOut.get());
            restoreSelectionByName(outputPortalNames, outputCompatiblePortals,
                                   selectedOutputPortalIndex, selectedOutputPortal,
                                   selectedOutputPortalName.get());
        });
        sizeListeners.push(outputWidth.newListener([this](int &){ recomputeMesh(); }));
        sizeListeners.push(outputHeight.newListener([this](int &){ recomputeMesh(); }));

        updateSelectedInputPortal();
        updateSelectedOutputPortal();
    }

    void update(ofEventArgs &) override {
        static int counter = 0;
        if(++counter % 60 == 0) {
            updatePortalList(inputPortalNames, inputCompatiblePortals, globalSearchIn.get(),
                             "ROI Warp Display", "Input Portal",
                             selectedInputPortalIndex, selectedInputPortal);
            updatePortalList(outputPortalNames, outputCompatiblePortals, globalSearchOut.get(),
                             "ROI Warp Display", "Output Portal",
                             selectedOutputPortalIndex, selectedOutputPortal);
        }

        if(needsDelayedRestore) {
            updatePortalListOnly(inputPortalNames, inputCompatiblePortals, globalSearchIn.get());
            restoreSelectionByName(inputPortalNames, inputCompatiblePortals,
                                   selectedInputPortalIndex, selectedInputPortal,
                                   selectedInputPortalName.get());
            updatePortalListOnly(outputPortalNames, outputCompatiblePortals, globalSearchOut.get());
            restoreSelectionByName(outputPortalNames, outputCompatiblePortals,
                                   selectedOutputPortalIndex, selectedOutputPortal,
                                   selectedOutputPortalName.get());
            needsDelayedRestore = false;
        }
    }

    void presetSave(ofJson &json) override {
        json["inputPortalName"]  = selectedInputPortal  ? selectedInputPortal->getName()  : "";
        json["outputPortalName"] = selectedOutputPortal ? selectedOutputPortal->getName() : "";
    }

    void presetRecallBeforeSettingParameters(ofJson &json) override {
        if(json.contains("inputPortalName")  && json["inputPortalName"].is_string())
            selectedInputPortalName.set(json["inputPortalName"].get<string>());
        if(json.contains("outputPortalName") && json["outputPortalName"].is_string())
            selectedOutputPortalName.set(json["outputPortalName"].get<string>());
    }

    void presetRecallAfterSettingParameters(ofJson &) override {
        needsDelayedRestore = true;
    }

    void draw(ofEventArgs &) override {
        ofTexture* inputTex = nullptr;
        if(selectedInputPortal) {
            try { inputTex = selectedInputPortal->getValue(); }
            catch(...) { selectedInputPortal = nullptr; }
        }
        if(!inputTex || !inputTex->isAllocated()) return;

        // Allocate output FBO
        if(!fbo.isAllocated() || fbo.getWidth() != outputWidth || fbo.getHeight() != outputHeight) {
            ofFbo::Settings s;
            s.width = outputWidth; s.height = outputHeight;
            s.internalformat = GL_RGBA32F;
            s.maxFilter = GL_NEAREST; s.minFilter = GL_NEAREST;
            s.numColorbuffers = 1;
            s.useDepth = false; s.useStencil = false;
            s.textureTarget = GL_TEXTURE_2D;
            fbo.allocate(s);
        }

        // Detect changes needing mesh rebuild
        bool isRect = inputTex->texData.textureTarget == GL_TEXTURE_RECTANGLE_ARB;
        bool needsRebuild = false;
        if(isRect != meshUsePixelCoords) { meshUsePixelCoords = isRect; needsRebuild = true; }
        if(inputTex->getWidth() != inputTexWidth || inputTex->getHeight() != inputTexHeight) {
            inputTexWidth  = inputTex->getWidth();
            inputTexHeight = inputTex->getHeight();
            needsRebuild = true;
        }
        if(!warpMesh.hasVertices()) needsRebuild = true;
        if(needsRebuild) recomputeMesh();

        // GL_TEXTURE_2D preview FBO for ImGui
        if(!previewFbo.isAllocated() ||
           previewFbo.getWidth()  != inputTexWidth ||
           previewFbo.getHeight() != inputTexHeight) {
            ofFbo::Settings ps;
            ps.width  = std::max((int)inputTexWidth, 1);
            ps.height = std::max((int)inputTexHeight, 1);
            ps.internalformat = GL_RGBA;
            ps.textureTarget  = GL_TEXTURE_2D;
            ps.useDepth = false; ps.useStencil = false;
            previewFbo.allocate(ps);
        }
        previewFbo.begin();
        ofClear(0, 0, 0, 255);
        ofPushStyle(); ofSetColor(255);
        inputTex->draw(0, 0, previewFbo.getWidth(), previewFbo.getHeight());
        ofPopStyle();
        previewFbo.end();

        // Render warped output
        fbo.begin();
        ofClear(0, 0, 0, 255);
        ofPushStyle(); ofSetColor(255);
        inputTex->bind();
        warpMesh.draw();
        inputTex->unbind();
        ofPopStyle();
        fbo.end();

        // Write to output portal
        if(selectedOutputPortal) {
            try { selectedOutputPortal->setValue(&fbo.getTexture()); }
            catch(...) { selectedOutputPortal = nullptr; }
        }
    }

private:

    // ---- Inspector canvas ----

    void drawDisplay() {
        float aspect = (inputTexWidth > 0 && inputTexHeight > 0)
                     ? (float)inputTexHeight / (float)inputTexWidth : 1.0f;
        float canvasW = previewWidth.get();
        float canvasH = canvasW * aspect;
        if(canvasW <= 0 || canvasH <= 0) return;

        ImVec2 screenPos  = ImGui::GetCursorScreenPos();
        ImVec2 screenSize = ImVec2(canvasW, canvasH);

        ImGui::InvisibleButton("##roiWarpGuiCanvas", screenSize);
        bool isHovered = ImGui::IsItemHovered();
        bool isActive  = ImGui::IsItemActive();

        bool pointsUpdated = false;

        if(isHovered || isActive) {
            bool mouseClicked = ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1);
            if(mouseClicked) {
                bool foundPoint = false;
                for(int i = 3; i >= 0 && !foundPoint; i--) {
                    auto pt = (warpPoints[i] * screenSize) + screenPos;
                    if(glm::distance(glm::vec2(ImGui::GetMousePos()), pt) < 10) {
                        pointDraggingIndex = i;
                        foundPoint = true;
                    }
                }
                if(!foundPoint) pointDraggingIndex = -1;
            } else if(ImGui::IsMouseDragging(0)) {
                if(pointDraggingIndex >= 0 && pointDraggingIndex < 4) {
                    if(ImGui::GetIO().KeyAlt)
                        warpPoints[pointDraggingIndex] += ImGui::GetIO().MouseDelta / (screenSize * ImVec2(100,100));
                    else
                        warpPoints[pointDraggingIndex] += ImGui::GetIO().MouseDelta / screenSize;
                    pointsUpdated = true;
                }
            } else if(pointDraggingIndex >= 0 && pointDraggingIndex < 4) {
                float mv = ImGui::GetIO().KeyAlt ? 0.00001f : 0.001f;
                if(ImGui::IsKeyDown(ImGuiKey_LeftArrow))       { warpPoints[pointDraggingIndex] += glm::vec2(-mv, 0); pointsUpdated = true; }
                else if(ImGui::IsKeyDown(ImGuiKey_RightArrow)) { warpPoints[pointDraggingIndex] += glm::vec2( mv, 0); pointsUpdated = true; }
                if(ImGui::IsKeyDown(ImGuiKey_UpArrow))         { warpPoints[pointDraggingIndex] += glm::vec2(0, -mv); pointsUpdated = true; }
                else if(ImGui::IsKeyDown(ImGuiKey_DownArrow))  { warpPoints[pointDraggingIndex] += glm::vec2(0,  mv); pointsUpdated = true; }
            }
            if(ImGui::IsKeyPressed(ImGuiKey_Tab))
                pointDraggingIndex = (pointDraggingIndex + 1) % 4;
            if(pointDraggingIndex >= 0 && pointDraggingIndex < 4) {
                warpPoints[pointDraggingIndex].x = glm::clamp(warpPoints[pointDraggingIndex].x, 0.0f, 1.0f);
                warpPoints[pointDraggingIndex].y = glm::clamp(warpPoints[pointDraggingIndex].y, 0.0f, 1.0f);
            }
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->PushClipRect(screenPos, screenPos + screenSize, true);

        if(previewFbo.isAllocated()) {
            ImTextureID tid = (ImTextureID)(uintptr_t)previewFbo.getTexture().texData.textureID;
            dl->AddImage(tid, screenPos, screenPos + screenSize,
                         ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255,255));
        } else {
            dl->AddRectFilled(screenPos, screenPos + screenSize, IM_COL32(50,50,50,255));
            const char* lbl = "No input texture";
            ImVec2 ts = ImGui::CalcTextSize(lbl);
            dl->AddText(ImVec2(screenPos.x + (canvasW - ts.x) * 0.5f,
                               screenPos.y + (canvasH - ts.y) * 0.5f),
                        IM_COL32(120,120,120,255), lbl);
        }
        for(int i = 0; i < 4; i++) {
            ImVec2 a = (warpPoints[i]         * screenSize) + screenPos;
            ImVec2 b = (warpPoints[(i+1) % 4] * screenSize) + screenPos;
            dl->AddLine(a, b, IM_COL32(255,128,0,200), 1.5f);
        }
        for(int i = 0; i < 4; i++) {
            ImVec2 pt    = (warpPoints[i] * screenSize) + screenPos;
            ImU32  color = (pointDraggingIndex == i) ? IM_COL32(255,255,0,255) : IM_COL32(255,128,0,255);
            dl->AddCircle(pt, 10, color);
            string ns = ofToString(i);
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                        pt - glm::vec2(5,5), IM_COL32(255,255,255,255),
                        ns.c_str(), ns.c_str() + (i < 10 ? 1 : 2));
        }
        dl->PopClipRect();

        if(pointsUpdated) recomputeMesh();
    }

    // ---- Portal helpers ----

    string stripName(const string& displayName) {
        string n = displayName;
        size_t sl = n.find_last_of('/');
        if(sl != string::npos) n = n.substr(sl + 1);
        if(n.size() >= 2 && n.substr(n.size() - 2) == " *") n = n.substr(0, n.size() - 2);
        return n;
    }

    void updatePortalListOnly(vector<string>& names,
                              vector<portal<ofTexture*>*>& portals,
                              bool globalSearch) {
        names.clear(); portals.clear();
        set<string> seen;
        string myScope = getParents();
        for(auto* p : ofxOceanodeShared::getAllPortals<ofTexture*>()) {
            if(!p) continue;
            bool ok = globalSearch ? true
                                   : (p->isLocal() ? p->getParents() == myScope : true);
            if(!ok) continue;
            string pname = p->getName();
            if(seen.count(pname)) continue;
            seen.insert(pname);
            string disp = pname;
            if(globalSearch) {
                string sc = p->getParents();
                if(!sc.empty() && sc != myScope) disp = sc + "/" + pname;
            }
            if(!p->isLocal()) disp += " *";
            names.push_back(disp);
            portals.push_back(p);
        }
        if(names.empty()) { names.push_back("No Compatible Portals"); portals.clear(); }
    }

    void updatePortalList(vector<string>& names,
                          vector<portal<ofTexture*>*>& portals,
                          bool globalSearch,
                          const string& nodeClass, const string& dropdownKey,
                          ofParameter<int>& indexParam,
                          portal<ofTexture*>*& instance) {
        vector<string> newNames;
        vector<portal<ofTexture*>*> newPortals;
        updatePortalListOnly(newNames, newPortals, globalSearch);
        if(newNames == names) return;

        string curName;
        if(indexParam >= 0 && indexParam < (int)names.size())
            curName = stripName(names[indexParam]);

        names   = newNames;
        portals = newPortals;
        try {
            ofxOceanodeInspectorController::registerInspectorDropdown(nodeClass, dropdownKey, names);
            indexParam.setMin(0);
            indexParam.setMax(std::max(0, (int)names.size() - 1));
        } catch(...) {}

        restoreSelectionByName(names, portals, indexParam, instance,
                               curName.empty() ? "" : curName);
    }

    void restoreSelectionByName(vector<string>& names,
                                vector<portal<ofTexture*>*>& portals,
                                ofParameter<int>& indexParam,
                                portal<ofTexture*>*& instance,
                                const string& name) {
        if(name.empty()) { maintainByInstance(names, portals, indexParam, instance); return; }
        for(int i = 0; i < (int)portals.size(); i++) {
            if(!portals[i]) continue;
            try {
                if(portals[i]->getName() == name) {
                    indexParam = i;
                    instance   = portals[i];
                    return;
                }
            } catch(...) {}
        }
        maintainByInstance(names, portals, indexParam, instance);
    }

    void maintainByInstance(vector<string>& names,
                            vector<portal<ofTexture*>*>& portals,
                            ofParameter<int>& indexParam,
                            portal<ofTexture*>*& instance) {
        if(instance) {
            for(int i = 0; i < (int)portals.size(); i++) {
                if(portals[i] == instance) { indexParam = i; return; }
            }
        }
        indexParam = 0;
        instance   = (!portals.empty() && portals[0]) ? portals[0] : nullptr;
    }

    void updateSelectedInputPortal() {
        int idx = selectedInputPortalIndex.get();
        if(idx >= 0 && idx < (int)inputCompatiblePortals.size() && inputCompatiblePortals[idx]) {
            selectedInputPortal = inputCompatiblePortals[idx];
            try {
                string n = selectedInputPortal->getName();
                if(selectedInputPortalName.get() != n) selectedInputPortalName.set(n);
            } catch(...) { selectedInputPortal = nullptr; selectedInputPortalName.set(""); }
        } else {
            selectedInputPortal = nullptr;
            selectedInputPortalName.set("");
        }
    }

    void updateSelectedOutputPortal() {
        int idx = selectedOutputPortalIndex.get();
        if(idx >= 0 && idx < (int)outputCompatiblePortals.size() && outputCompatiblePortals[idx]) {
            selectedOutputPortal = outputCompatiblePortals[idx];
            try {
                string n = selectedOutputPortal->getName();
                if(selectedOutputPortalName.get() != n) selectedOutputPortalName.set(n);
            } catch(...) { selectedOutputPortal = nullptr; selectedOutputPortalName.set(""); }
        } else {
            selectedOutputPortal = nullptr;
            selectedOutputPortalName.set("");
        }
    }

    // ---- Homography mesh ----

    void recomputeMesh() {
        if(inputTexWidth <= 0 || inputTexHeight <= 0) return;
        glm::vec2 outCorners[4] = {
            {0.f,              0.f             },
            {(float)outputWidth,  0.f          },
            {(float)outputWidth,  (float)outputHeight},
            {0.f,              (float)outputHeight}
        };
        glm::vec2 inPoints[4] = {
            warpPoints[0] * glm::vec2(inputTexWidth, inputTexHeight),
            warpPoints[1] * glm::vec2(inputTexWidth, inputTexHeight),
            warpPoints[2] * glm::vec2(inputTexWidth, inputTexHeight),
            warpPoints[3] * glm::vec2(inputTexWidth, inputTexHeight),
        };
        ofMatrix4x4 invH = ofxHomography::findHomography(outCorners, inPoints);
        float tcX = meshUsePixelCoords ? 1.f : 1.f / inputTexWidth;
        float tcY = meshUsePixelCoords ? 1.f : 1.f / inputTexHeight;
        const int N = 32;
        warpMesh.clear();
        warpMesh.setMode(OF_PRIMITIVE_TRIANGLES);
        for(int j = 0; j <= N; j++) {
            for(int i = 0; i <= N; i++) {
                float u = (float)i/N, v = (float)j/N;
                float ox = u * outputWidth, oy = v * outputHeight;
                ofPoint p = ofxHomography::toScreenCoordinates(ofPoint(ox, oy, 0), invH);
                warpMesh.addVertex(glm::vec3(ox, oy, 0));
                warpMesh.addTexCoord(glm::vec2(p.x * tcX, p.y * tcY));
            }
        }
        for(int j = 0; j < N; j++) {
            for(int i = 0; i < N; i++) {
                int idx = j*(N+1)+i;
                warpMesh.addIndex(idx);     warpMesh.addIndex(idx+1);
                warpMesh.addIndex(idx+N+1); warpMesh.addIndex(idx+1);
                warpMesh.addIndex(idx+N+2); warpMesh.addIndex(idx+N+1);
            }
        }
    }

    // ---- Inspector parameters ----
    ofParameter<int>    outputWidth, outputHeight;
    ofParameter<float>  previewWidth;
    ofParameter<bool>   globalSearchIn, globalSearchOut;
    ofParameter<string> selectedInputPortalName, selectedOutputPortalName;
    ofParameter<int>    selectedInputPortalIndex, selectedOutputPortalIndex;

    // ---- Portal state ----
    vector<string>              inputPortalNames,  outputPortalNames;
    vector<portal<ofTexture*>*> inputCompatiblePortals, outputCompatiblePortals;
    portal<ofTexture*>*         selectedInputPortal,  *selectedOutputPortal;

    // ---- Warp state ----
    float     inputTexWidth, inputTexHeight;
    bool      meshUsePixelCoords;
    int       pointDraggingIndex;
    glm::vec2 warpPoints[4];

    // ---- FBO / mesh ----
    ofFbo  fbo, previewFbo;
    ofMesh warpMesh;

    // ---- Misc ----
    customGuiRegion  displayRegion;
    ofEventListeners sizeListeners;
    ofEventListener  dropdownInListener, dropdownOutListener;
    ofEventListener  globalSearchInListener, globalSearchOutListener;
    ofEventListener  presetLoadedListener;
    bool             needsDelayedRestore;
};

#endif /* ROIWarpGuiDisplay_h */
