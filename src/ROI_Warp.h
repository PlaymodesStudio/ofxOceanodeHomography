//
//  ROI_Warp.h
//  ofxOceanode
//
//  Created by Eduard Frigola on 4/9/25.
//

#ifndef ROI_Warp_h
#define ROI_Warp_h

#include "ofMain.h"
#include "ofxOceanodeNodeModel.h"
#include "ofxHomography.h"

class ROI_Warp : public ofxOceanodeNodeModel {
public:
    ROI_Warp() : ofxOceanodeNodeModel("ROI Warp"){}

    void setup() override {
        addParameter(input.set("Input", nullptr));
        addParameter(width.set("Width", 1920, 1, INT_MAX));
        addParameter(height.set("Height", 1080, 1, INT_MAX));
        addParameter(showWindow.set("Show", true));
        addOutputParameter(output.set("Output", nullptr));

        inputWidth = 0;
        inputHeight = 0;
        meshUsePixelCoords = false;
        pointDraggingIndex = -1;

        // Default: full image corners (TL, TR, BR, BL) in normalized input space
        warpPoints[0] = glm::vec2(0, 0);
        warpPoints[1] = glm::vec2(1, 0);
        warpPoints[2] = glm::vec2(1, 1);
        warpPoints[3] = glm::vec2(0, 1);

        listeners.push(width.newListener([this](int &){ recomputeMesh(); }));
        listeners.push(height.newListener([this](int &){ recomputeMesh(); }));
    }

    void draw(ofEventArgs &a) override {
        if(input.get() != nullptr) {
            // Allocate / reallocate output FBO
            if(!fbo.isAllocated() || fbo.getWidth() != width || fbo.getHeight() != height) {
                ofFbo::Settings settings;
                settings.height = height;
                settings.width = width;
                settings.internalformat = GL_RGBA32F;
                settings.maxFilter = GL_NEAREST;
                settings.minFilter = GL_NEAREST;
                settings.numColorbuffers = 1;
                settings.useDepth = false;
                settings.useStencil = false;
                settings.textureTarget = GL_TEXTURE_2D;
                fbo.allocate(settings);
            }

            // Detect texture state changes that require a mesh rebuild
            bool isRect = input.get()->texData.textureTarget == GL_TEXTURE_RECTANGLE_ARB;
            bool needsRebuild = false;

            if(isRect != meshUsePixelCoords) {
                meshUsePixelCoords = isRect;
                needsRebuild = true;
            }
            if(input.get()->getWidth() != inputWidth || input.get()->getHeight() != inputHeight) {
                inputWidth  = input.get()->getWidth();
                inputHeight = input.get()->getHeight();
                needsRebuild = true;
            }
            if(!warpMesh.hasVertices()) needsRebuild = true;

            if(needsRebuild) recomputeMesh();

            // Allocate / reallocate input preview FBO (always GL_TEXTURE_2D for ImGui)
            if(!previewFbo.isAllocated() ||
               previewFbo.getWidth()  != inputWidth ||
               previewFbo.getHeight() != inputHeight) {
                ofFbo::Settings ps;
                ps.width  = std::max((int)inputWidth, 1);
                ps.height = std::max((int)inputHeight, 1);
                ps.internalformat = GL_RGBA;
                ps.textureTarget  = GL_TEXTURE_2D;
                ps.useDepth   = false;
                ps.useStencil = false;
                previewFbo.allocate(ps);
            }

            // Render input into preview FBO
            previewFbo.begin();
            ofClear(0, 0, 0, 255);
            ofPushStyle();
            ofSetColor(255, 255, 255, 255);
            input.get()->draw(0, 0, previewFbo.getWidth(), previewFbo.getHeight());
            ofPopStyle();
            previewFbo.end();

            // Render warped output
            fbo.begin();
            ofClear(0, 0, 0, 255);
            ofPushStyle();
            ofSetColor(255, 255, 255, 255);
            input.get()->bind();
            warpMesh.draw();
            input.get()->unbind();
            ofPopStyle();
            fbo.end();

            output = &fbo.getTexture();
        }

        // ---- ImGui window ----
        if(showWindow.get()) {
            string modCanvasID = canvasID == "Canvas" ? "" : (canvasID + "/");

            // Aspect ratio from input texture (stable, no feedback loop)
            float aspect = (inputWidth > 0 && inputHeight > 0) ? inputHeight / inputWidth : 1.0f;
            // Title bar + padding delta — computed from style, never from measured window dims
            float extraH = ImGui::GetFrameHeight()
                         + (ImGui::GetStyle().WindowPadding.y - ImGui::GetStyle().WindowPadding.x) * 2.0f;

            float constraintData[2] = { aspect, extraH };
            ImGui::SetNextWindowSizeConstraints(
                ImVec2(50, 50 + extraH), ImVec2(FLT_MAX, FLT_MAX),
                [](ImGuiSizeCallbackData* d){
                    float* p = (float*)d->UserData;
                    d->DesiredSize.y = d->DesiredSize.x * p[0] + p[1];
                },
                constraintData);

            if(ImGui::Begin(("ROI Warp_" + modCanvasID + "Curve " + ofToString(getNumIdentifier())).c_str(), nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                bool pointsUpdated = false;

                float canvasW = ImGui::GetContentRegionAvail().x;
                float canvasH = canvasW * aspect;

            // Capture position before the button so it is always in screen space
            ImVec2 screenPos  = ImGui::GetCursorScreenPos();
            ImVec2 screenSize = ImVec2(canvasW, canvasH);

            ImGui::InvisibleButton("##roiWarpCanvas", screenSize);
            bool isHovered = ImGui::IsItemHovered();
            bool isActive  = ImGui::IsItemActive();

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
                        if(ImGui::GetIO().KeyAlt) {
                            warpPoints[pointDraggingIndex] += ImGui::GetIO().MouseDelta / (screenSize * ImVec2(100, 100));
                        } else {
                            warpPoints[pointDraggingIndex] += ImGui::GetIO().MouseDelta / screenSize;
                        }
                        pointsUpdated = true;
                    }
                } else if(pointDraggingIndex >= 0 && pointDraggingIndex < 4) {
                    float moveAmt = ImGui::GetIO().KeyAlt ? 0.00001f : 0.001f;
                    if(ImGui::IsKeyDown(ImGuiKey_LeftArrow))  { warpPoints[pointDraggingIndex] += glm::vec2(-moveAmt, 0); pointsUpdated = true; }
                    else if(ImGui::IsKeyDown(ImGuiKey_RightArrow)) { warpPoints[pointDraggingIndex] += glm::vec2(moveAmt, 0); pointsUpdated = true; }
                    if(ImGui::IsKeyDown(ImGuiKey_UpArrow))    { warpPoints[pointDraggingIndex] += glm::vec2(0, -moveAmt); pointsUpdated = true; }
                    else if(ImGui::IsKeyDown(ImGuiKey_DownArrow))  { warpPoints[pointDraggingIndex] += glm::vec2(0, moveAmt); pointsUpdated = true; }
                }

                if(ImGui::IsKeyPressed(ImGuiKey_Tab)) {
                    pointDraggingIndex = (pointDraggingIndex + 1) % 4;
                }

                if(pointDraggingIndex >= 0 && pointDraggingIndex < 4) {
                    warpPoints[pointDraggingIndex].x = glm::clamp(warpPoints[pointDraggingIndex].x, 0.0f, 1.0f);
                    warpPoints[pointDraggingIndex].y = glm::clamp(warpPoints[pointDraggingIndex].y, 0.0f, 1.0f);
                }
            }

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->PushClipRect(screenPos, screenPos + screenSize, true);

            // Show input preview (always GL_TEXTURE_2D)
            if(previewFbo.isAllocated()) {
                ImTextureID texID = (ImTextureID)(uintptr_t)previewFbo.getTexture().texData.textureID;
                draw_list->AddImage(texID, screenPos, screenPos + screenSize,
                                    ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255,255));
            } else {
                draw_list->AddRectFilled(screenPos, screenPos + screenSize, IM_COL32(50,50,50,255));
            }

            // Draw quadrilateral outline
            for(int i = 0; i < 4; i++) {
                ImVec2 a = (warpPoints[i]        * screenSize) + screenPos;
                ImVec2 b = (warpPoints[(i+1) % 4] * screenSize) + screenPos;
                draw_list->AddLine(a, b, IM_COL32(255, 128, 0, 200), 1.5f);
            }

            // Draw control point handles
            for(int i = 0; i < 4; i++) {
                ImVec2 pt = (warpPoints[i] * screenSize) + screenPos;
                ImU32 color = (pointDraggingIndex == i) ? IM_COL32(255,255,0,255) : IM_COL32(255,128,0,255);
                draw_list->AddCircle(pt, 10, color);
                string numStr = ofToString(i);
                draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                                   pt - glm::vec2(5, 5), IM_COL32(255,255,255,255),
                                   numStr.c_str(), numStr.c_str() + (i < 10 ? 1 : 2));
            }

            draw_list->PopClipRect();

            if(pointsUpdated) recomputeMesh();
            }
            ImGui::End();
        }
    }

private:

    // Builds a subdivided mesh that implements the inverse homography:
    //   for each output pixel → find the corresponding input pixel.
    void recomputeMesh() {
        if(inputWidth <= 0 || inputHeight <= 0) return;

        // Inverse homography: output pixel coords → input pixel coords
        glm::vec2 outputCorners[4] = {
            {0.0f,           0.0f           },
            {(float)width,   0.0f           },
            {(float)width,   (float)height  },
            {0.0f,           (float)height  }
        };
        glm::vec2 inputPoints[4] = {
            warpPoints[0] * glm::vec2(inputWidth, inputHeight),
            warpPoints[1] * glm::vec2(inputWidth, inputHeight),
            warpPoints[2] * glm::vec2(inputWidth, inputHeight),
            warpPoints[3] * glm::vec2(inputWidth, inputHeight),
        };

        ofMatrix4x4 invH = ofxHomography::findHomography(outputCorners, inputPoints);

        // Texture coordinate scale: pixel coords for TEXTURE_RECTANGLE, normalised for TEXTURE_2D
        float tcScaleX = meshUsePixelCoords ? 1.0f : 1.0f / inputWidth;
        float tcScaleY = meshUsePixelCoords ? 1.0f : 1.0f / inputHeight;

        const int N = 32; // subdivisions per axis
        warpMesh.clear();
        warpMesh.setMode(OF_PRIMITIVE_TRIANGLES);

        for(int j = 0; j <= N; j++) {
            for(int i = 0; i <= N; i++) {
                float u  = (float)i / N;
                float v  = (float)j / N;
                float ox = u * width;
                float oy = v * height;

                ofPoint inPt = ofxHomography::toScreenCoordinates(ofPoint(ox, oy, 0), invH);

                warpMesh.addVertex(glm::vec3(ox, oy, 0));
                warpMesh.addTexCoord(glm::vec2(inPt.x * tcScaleX, inPt.y * tcScaleY));
            }
        }

        for(int j = 0; j < N; j++) {
            for(int i = 0; i < N; i++) {
                int idx = j * (N + 1) + i;
                warpMesh.addIndex(idx);
                warpMesh.addIndex(idx + 1);
                warpMesh.addIndex(idx + N + 1);
                warpMesh.addIndex(idx + 1);
                warpMesh.addIndex(idx + N + 2);
                warpMesh.addIndex(idx + N + 1);
            }
        }
    }

    ofParameter<ofTexture*> input;
    ofParameter<ofTexture*> output;
    ofParameter<int> width;
    ofParameter<int> height;
    ofParameter<bool> showWindow;

    float inputWidth;
    float inputHeight;
    bool  meshUsePixelCoords;

    ofEventListeners listeners;

    ofFbo    fbo;        // warped output
    ofFbo    previewFbo; // GL_TEXTURE_2D copy of input for ImGui display
    ofMesh   warpMesh;

    glm::vec2 warpPoints[4]; // normalised (0–1) positions on the input image
    int        pointDraggingIndex;
};

#endif /* ROI_Warp_h */
