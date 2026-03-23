//
//  cornerPin.h
//  ofxOceanode
//
//  Created by Eduard Frigola on 4/9/25.
//

#ifndef cornerPin_h
#define cornerPin_h

#include "ofMain.h"
#include "ofxOceanodeNodeModel.h"
#include "ofxHomography.h"

class cornerPin : public ofxOceanodeNodeModel {
public:
    cornerPin() : ofxOceanodeNodeModel("Corner Pin"){}
    
    void setup() override{
        addParameter(input.set("Input", nullptr));
        addParameter(width.set("Width", 100, 1, INT_MAX));
        addParameter(height.set("Height", 100, 1, INT_MAX));
        addParameter(guideWidth.set("Guide Width", 0, 0, 10));
        addParameter(showWindow.set("Show", true));
        addOutputParameter(output.set("Output", nullptr));

        inputWidth = 100;
        inputHeight = 100;
        
        originalCorners[0] = glm::vec2(0, 0);
        originalCorners[1] = glm::vec2(inputWidth, 0);
        originalCorners[2] = glm::vec2(inputWidth, inputHeight);
        originalCorners[3] = glm::vec2(0, inputHeight);
        
        distortedCorners[0] = glm::vec2(0, 0);
        distortedCorners[1] = glm::vec2(width, 0);
        distortedCorners[2] = glm::vec2(width, height);
        distortedCorners[3] = glm::vec2(0, height);
        
        warpPoints[0] = glm::vec2(0, 0);
        warpPoints[1] = glm::vec2(1, 0);
        warpPoints[2] = glm::vec2(1, 1);
        warpPoints[3] = glm::vec2(0, 1);
        
        pointDraggingIndex = -1;
        
        listeners.push(width.newListener([this](int &i){
            recomputeHomography();
        }));
        
        listeners.push(height.newListener([this](int &i){
            recomputeHomography();
        }));
    }
    
	void draw(ofEventArgs &a) override{
		if(input.get() != nullptr){
			if(!fbo.isAllocated() || fbo.getWidth() != width || fbo.getHeight() != height){
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
			
			if(input.get()->getWidth() != inputWidth || input.get()->getHeight() != inputHeight){
				inputWidth = input.get()->getWidth();
				inputHeight = input.get()->getHeight();
				
				recomputeHomography();
			}
			
			fbo.begin();
			ofClear(0, 0, 0, 255);
			ofPushStyle();
			ofSetColor(255, 255, 255, 255);
			
			ofPushMatrix();
			ofMultMatrix(homography);
			input.get()->draw(0, 0);
			ofPopMatrix();
			
			if(guideWidth != 0 && pointDraggingIndex >= 0 && pointDraggingIndex < 4){
				ofSetColor(255, 0, 0, 255);
				int x = distortedCorners[pointDraggingIndex].x;
				int y = distortedCorners[pointDraggingIndex].y;
				ofSetLineWidth(guideWidth);
				ofDrawLine(x, 0, x, height);
				ofDrawLine(0, y, width, y);
			}
			
			ofPopStyle();
			fbo.end();
			
			output = &fbo.getTexture();
		}
		
		if(showWindow.get()){
			string modCanvasID = canvasID == "Canvas" ? "" : (canvasID + "/");

			// Aspect ratio from output dimensions (stable, no feedback loop)
			float aspect = (width > 0 && height > 0) ? (float)height / (float)width : 1.0f;
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

			if(ImGui::Begin(("Corner Pin_" + modCanvasID + "Curve " + ofToString(getNumIdentifier())).c_str(), nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)){
				bool pointsUpdated = false;

				float canvasW = ImGui::GetContentRegionAvail().x;
				float canvasH = canvasW * aspect;

			// Capture position before the button so it is always in screen space
			ImVec2 screenPos  = ImGui::GetCursorScreenPos();
			ImVec2 screenSize = ImVec2(canvasW, canvasH);

			ImGui::InvisibleButton("##warpCanvas", screenSize);
			bool isHovered = ImGui::IsItemHovered();
			bool isActive  = ImGui::IsItemActive();

			if(isHovered || isActive){
				bool mouseClicked = ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1);
				if(mouseClicked){
					bool foundPoint = false;
					for(int i = 4-1; i >= 0 && !foundPoint ; i--){
						auto point = (warpPoints[i] * screenSize) + screenPos;
						if(glm::distance(glm::vec2(ImGui::GetMousePos()), point) < 10){
							pointDraggingIndex = i;
							foundPoint = true;
						}
					}
					if(!foundPoint){
						pointDraggingIndex = -1;
					}
				}
				else if(ImGui::IsMouseDragging(0)){
					if(pointDraggingIndex >= 0 && pointDraggingIndex < 4){
						if(ImGui::GetIO().KeyAlt){
							warpPoints[pointDraggingIndex] += ImGui::GetIO().MouseDelta / (screenSize * ImVec2(100, 100));
							pointsUpdated = true;
						}else{
							warpPoints[pointDraggingIndex] += ImGui::GetIO().MouseDelta / screenSize;
							pointsUpdated = true;
						}
					}
				}else if(pointDraggingIndex >= 0 && pointDraggingIndex < 4){
					float moveAmt = ImGui::GetIO().KeyAlt ? 0.00001 : 0.001;
					if(ImGui::IsKeyDown(ImGuiKey_LeftArrow)){
						warpPoints[pointDraggingIndex] += glm::vec2(-moveAmt, 0);
						pointsUpdated = true;
					}else if(ImGui::IsKeyDown(ImGuiKey_RightArrow)){
						warpPoints[pointDraggingIndex] += glm::vec2(moveAmt, 0);
						pointsUpdated = true;
					}if(ImGui::IsKeyDown(ImGuiKey_UpArrow)){
						warpPoints[pointDraggingIndex] += glm::vec2(0, -moveAmt);
						pointsUpdated = true;
					}else if(ImGui::IsKeyDown(ImGuiKey_DownArrow)){
						warpPoints[pointDraggingIndex] += glm::vec2(0, moveAmt);
						pointsUpdated = true;
					}
				}
				if(ImGui::IsKeyPressed(ImGuiKey_Tab)){
					pointDraggingIndex = (pointDraggingIndex + 1) % 4;
				}

				if(pointDraggingIndex >= 0 && pointDraggingIndex < 4){
					warpPoints[pointDraggingIndex].x = glm::clamp(warpPoints[pointDraggingIndex].x, 0.0f, 1.0f);
					warpPoints[pointDraggingIndex].y = glm::clamp(warpPoints[pointDraggingIndex].y, 0.0f, 1.0f);
				}
			}

			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			draw_list->PushClipRect(screenPos, screenPos + screenSize, true);

			if(fbo.isAllocated()) {
				ImTextureID textureID = (ImTextureID)(uintptr_t)fbo.getTexture().texData.textureID;
				draw_list->AddImage(textureID, screenPos, screenPos + screenSize, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(180, 180, 180, 255));
			} else {
				draw_list->AddRectFilled(screenPos, screenPos + screenSize, IM_COL32(50, 50, 50, 255));
			}

			for(int i = 0; i < 4; i++){
				ImVec2 pt = (warpPoints[i] * screenSize) + screenPos;
				ImU32 color = (pointDraggingIndex == i) ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 128, 0, 255);
				draw_list->AddCircle(pt, 10, color);
				string numString = ofToString(i);
				draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), pt - glm::vec2(5, 5), IM_COL32(255,255,255,255), numString.c_str(), numString.c_str() + (i < 10 ? 1 : 2));
			}

			draw_list->PopClipRect();

				if(pointsUpdated){
				recomputeHomography();
			}
			}
			ImGui::End();
		}
	}
    
private:
    
    void recomputeHomography(){
        originalCorners[0] = glm::vec2(0, 0);
        originalCorners[1] = glm::vec2(inputWidth, 0);
        originalCorners[2] = glm::vec2(inputWidth, inputHeight);
        originalCorners[3] = glm::vec2(0, inputHeight);
        
        distortedCorners[0] = warpPoints[0] * glm::vec2(width, height);
        distortedCorners[1] = warpPoints[1] * glm::vec2(width, height);
        distortedCorners[2] = warpPoints[2] * glm::vec2(width, height);
        distortedCorners[3] = warpPoints[3] * glm::vec2(width, height);
        
        homography = ofxHomography::findHomography(originalCorners, distortedCorners);
    }
    
    ofParameter<ofTexture*> input;
    ofParameter<ofTexture*> output;
    ofParameter<int> width;
    ofParameter<int> height;
    ofParameter<int> guideWidth;
    ofParameter<bool> showWindow;
    
    int inputWidth;
    int inputHeight;
    
    ofEventListeners listeners;
    
    ofFbo fbo;
    
    glm::mat4 homography;
    glm::vec2 originalCorners[4];
    glm::vec2 distortedCorners[4];
    
    glm::vec2 warpPoints[4];
    int pointDraggingIndex;
};

#endif /* cornerPin_h */
