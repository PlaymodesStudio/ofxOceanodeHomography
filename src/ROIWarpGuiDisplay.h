//
//  ROIWarpGuiDisplay.h
//  ofxOceanode
//
//  Like ROI_Warp but rendered entirely inside the node inspector as a
//  custom region — no separate floating window.
//

#ifndef ROIWarpGuiDisplay_h
#define ROIWarpGuiDisplay_h

#include "ofMain.h"
#include "ofxOceanodeNodeModel.h"
#include "ofxHomography.h"

class ROIWarpGuiDisplay : public ofxOceanodeNodeModel {
public:
    ROIWarpGuiDisplay() : ofxOceanodeNodeModel("ROI Warp Display"){}

    void setup() override {
        // NoGuiWidget: keeps the port pin for canvas connections, hides the text widget
        addParameter(input.set("Input",   nullptr), ofxOceanodeParameterFlags_NoGuiWidget);
        addParameter(width.set("Width",   1920, 1, INT_MAX), ofxOceanodeParameterFlags_NoGuiWidget);
        addParameter(height.set("Height", 1080, 1, INT_MAX), ofxOceanodeParameterFlags_NoGuiWidget);
        addOutputParameter(output.set("Output", nullptr), ofxOceanodeParameterFlags_NoGuiWidget);

        setFlags(ofxOceanodeNodeModelFlags_TransparentNode);

        // Fixed preview width in the inspector (mirrors textureDisplay pattern)
        addInspectorParameter(previewWidth.set("Preview Width", 400.f, 50.f, 1920.f));

        inputWidth  = 0;
        inputHeight = 0;
        meshUsePixelCoords = false;
        pointDraggingIndex = -1;

        warpPoints[0] = glm::vec2(0, 0);
        warpPoints[1] = glm::vec2(1, 0);
        warpPoints[2] = glm::vec2(1, 1);
        warpPoints[3] = glm::vec2(0, 1);

        listeners.push(width.newListener([this](int &){ recomputeMesh(); }));
        listeners.push(height.newListener([this](int &){ recomputeMesh(); }));

        addCustomRegion(displayRegion.set("Display", [this](){ drawDisplay(); }),
                        [this](){ drawDisplay(); });
    }

    void draw(ofEventArgs &) override {
        if(input.get() == nullptr) return;

        // Allocate / reallocate output FBO
        if(!fbo.isAllocated() || fbo.getWidth() != width || fbo.getHeight() != height) {
            ofFbo::Settings s;
            s.width  = width;  s.height = height;
            s.internalformat = GL_RGBA32F;
            s.maxFilter = GL_NEAREST;  s.minFilter = GL_NEAREST;
            s.numColorbuffers = 1;
            s.useDepth = false;  s.useStencil = false;
            s.textureTarget = GL_TEXTURE_2D;
            fbo.allocate(s);
        }

        // Detect changes that require a mesh rebuild
        bool isRect = input.get()->texData.textureTarget == GL_TEXTURE_RECTANGLE_ARB;
        bool needsRebuild = false;

        if(isRect != meshUsePixelCoords) { meshUsePixelCoords = isRect; needsRebuild = true; }
        if(input.get()->getWidth()  != inputWidth ||
           input.get()->getHeight() != inputHeight) {
            inputWidth  = input.get()->getWidth();
            inputHeight = input.get()->getHeight();
            needsRebuild = true;
        }
        if(!warpMesh.hasVertices()) needsRebuild = true;
        if(needsRebuild) recomputeMesh();

        // GL_TEXTURE_2D preview FBO for ImGui
        if(!previewFbo.isAllocated() ||
           previewFbo.getWidth()  != inputWidth ||
           previewFbo.getHeight() != inputHeight) {
            ofFbo::Settings ps;
            ps.width  = std::max((int)inputWidth,  1);
            ps.height = std::max((int)inputHeight, 1);
            ps.internalformat = GL_RGBA;
            ps.textureTarget  = GL_TEXTURE_2D;
            ps.useDepth = false;  ps.useStencil = false;
            previewFbo.allocate(ps);
        }

        previewFbo.begin();
        ofClear(0, 0, 0, 255);
        ofPushStyle(); ofSetColor(255);
        input.get()->draw(0, 0, previewFbo.getWidth(), previewFbo.getHeight());
        ofPopStyle();
        previewFbo.end();

        fbo.begin();
        ofClear(0, 0, 0, 255);
        ofPushStyle(); ofSetColor(255);
        input.get()->bind();
        warpMesh.draw();
        input.get()->unbind();
        ofPopStyle();
        fbo.end();

        output = &fbo.getTexture();
    }

private:

    // ---- Inspector canvas ----

    void drawDisplay() {
        float aspect  = (inputWidth > 0 && inputHeight > 0)
                      ? (float)inputHeight / (float)inputWidth : 1.0f;
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
                        warpPoints[pointDraggingIndex] += ImGui::GetIO().MouseDelta / (screenSize * ImVec2(100, 100));
                    else
                        warpPoints[pointDraggingIndex] += ImGui::GetIO().MouseDelta / screenSize;
                    pointsUpdated = true;
                }
            } else if(pointDraggingIndex >= 0 && pointDraggingIndex < 4) {
                float mv = ImGui::GetIO().KeyAlt ? 0.00001f : 0.001f;
                if(ImGui::IsKeyDown(ImGuiKey_LeftArrow))  { warpPoints[pointDraggingIndex] += glm::vec2(-mv, 0); pointsUpdated = true; }
                else if(ImGui::IsKeyDown(ImGuiKey_RightArrow)) { warpPoints[pointDraggingIndex] += glm::vec2(mv, 0);  pointsUpdated = true; }
                if(ImGui::IsKeyDown(ImGuiKey_UpArrow))    { warpPoints[pointDraggingIndex] += glm::vec2(0, -mv); pointsUpdated = true; }
                else if(ImGui::IsKeyDown(ImGuiKey_DownArrow))  { warpPoints[pointDraggingIndex] += glm::vec2(0, mv);  pointsUpdated = true; }
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

    // ---- Homography mesh ----

    void recomputeMesh() {
        if(inputWidth <= 0 || inputHeight <= 0) return;

        glm::vec2 outputCorners[4] = {
            {0.0f,         0.0f        },
            {(float)width, 0.0f        },
            {(float)width, (float)height},
            {0.0f,         (float)height}
        };
        glm::vec2 inputPoints[4] = {
            warpPoints[0] * glm::vec2(inputWidth, inputHeight),
            warpPoints[1] * glm::vec2(inputWidth, inputHeight),
            warpPoints[2] * glm::vec2(inputWidth, inputHeight),
            warpPoints[3] * glm::vec2(inputWidth, inputHeight),
        };

        ofMatrix4x4 invH = ofxHomography::findHomography(outputCorners, inputPoints);

        float tcScaleX = meshUsePixelCoords ? 1.0f : 1.0f / inputWidth;
        float tcScaleY = meshUsePixelCoords ? 1.0f : 1.0f / inputHeight;

        const int N = 32;
        warpMesh.clear();
        warpMesh.setMode(OF_PRIMITIVE_TRIANGLES);

        for(int j = 0; j <= N; j++) {
            for(int i = 0; i <= N; i++) {
                float u = (float)i / N,  v = (float)j / N;
                float ox = u * width,    oy = v * height;
                ofPoint inPt = ofxHomography::toScreenCoordinates(ofPoint(ox, oy, 0), invH);
                warpMesh.addVertex(glm::vec3(ox, oy, 0));
                warpMesh.addTexCoord(glm::vec2(inPt.x * tcScaleX, inPt.y * tcScaleY));
            }
        }
        for(int j = 0; j < N; j++) {
            for(int i = 0; i < N; i++) {
                int idx = j * (N+1) + i;
                warpMesh.addIndex(idx);          warpMesh.addIndex(idx+1);
                warpMesh.addIndex(idx+N+1);      warpMesh.addIndex(idx+1);
                warpMesh.addIndex(idx+N+2);      warpMesh.addIndex(idx+N+1);
            }
        }
    }

    // ---- Parameters ----
    ofParameter<ofTexture*> input;
    ofParameter<ofTexture*> output;
    ofParameter<int>        width;
    ofParameter<int>        height;
    ofParameter<float>      previewWidth;

    // ---- State ----
    float  inputWidth, inputHeight;
    bool   meshUsePixelCoords;
    int    pointDraggingIndex;
    glm::vec2 warpPoints[4];

    ofEventListeners listeners;
    ofFbo   fbo, previewFbo;
    ofMesh  warpMesh;

    customGuiRegion displayRegion;
};

#endif /* ROIWarpGuiDisplay_h */
