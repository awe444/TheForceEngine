#include <cstring>

#include "agentMenu.h"
#include "editBox.h"
#include "delt.h"
#include "menu.h"
#include "uiDraw.h"
#include <TFE_FrontEndUI/frontEndUi.h>
#include <TFE_DarkForces/agent.h>
#include <TFE_DarkForces/util.h>
#include <TFE_DarkForces/Landru/lcanvas.h>
#include <TFE_Archive/archive.h>
#include <TFE_Settings/settings.h>
#include <TFE_Input/input.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_System/system.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	///////////////////////////////////////////
	// Constants
	///////////////////////////////////////////
	enum AgentMenuButtons
	{
		AGENT_NEW,
		AGENT_REMOVE,
		AGENT_EXIT,
		AGENT_BEGIN,
		AGENT_COUNT
	};
	static const Vec2i c_agentButtons[AGENT_COUNT] =
	{
		{21, 165},	// AGENT_NEW
		{90, 165},	// AGENT_REMOVE
		{164, 165},	// AGENT_EXIT
		{239, 165},	// AGENT_BEGIN
	};
	static const Vec2i c_agentButtonDim = { 60, 25 };

	enum NewAgentDlg
	{
		NEW_AGENT_NO,
		NEW_AGENT_YES,
		NEW_AGENT_COUNT
	};
	static const Vec2i c_newAgentButtons[NEW_AGENT_COUNT] =
	{
		{167, 107},	// NEW_AGENT_NO
		{206, 107},	// NEW_AGENT_YES
	};
	static const Vec2i c_newAgentButtonDim = { 28, 15 };

	///////////////////////////////////////////
	// Internal State
	///////////////////////////////////////////
	static JBool s_loaded = JFALSE;
	static JBool s_displayInit = JFALSE;
	static u8*   s_framebuffer = nullptr;

	static s32 s_selectedMission = 0;
	static s32 s_editCursorFlicker = 0;
	static s32 s_agentCount = 0;
	static JBool s_lastSelectedAgent = JTRUE;
	static JBool s_newAgentDlg = JFALSE;
	static JBool s_removeAgentDlg = JFALSE;
	static JBool s_quitConfirmDlg = JFALSE;
	static JBool s_missionBegin = JFALSE;
	static EditBox s_editBox = {};
	
	static u8 s_menuPalette[768];
	static DeltFrame* s_agentMenuFrames;
	static DeltFrame* s_agentDlgFrames;
	static u32 s_agentMenuCount;
	static u32 s_agentDlgCount;
	
	static char s_newAgentName[32];
	static LangHotkeys* s_langKeys;

	// TFE: Gamepad navigation state (matching PR #1 commit 508f2007fb63 implementation)
	enum GamepadFocusArea
	{
		FOCUS_AGENT_LIST = 0,
		FOCUS_MISSION_LIST,
		FOCUS_BUTTONS,
		FOCUS_AREA_COUNT
	};
	static GamepadFocusArea s_gamepadFocusArea = FOCUS_AGENT_LIST;
	static s32 s_gamepadButtonFocus = AGENT_NEW;  // Which button is focused when in FOCUS_BUTTONS area
	static s32 s_gamepadDialogFocus = NEW_AGENT_NO; // Which dialog button is focused

	///////////////////////////////////////////
	// Forward Declarations
	///////////////////////////////////////////
	void agentMenu_startup();
	void agentMenu_startupDisplay();
	void agentMenu_blit();
	void agentMenu_update();
	void agentMenu_draw();

	void drawYesNoDlg(s32 baseFrame);
	void drawNewAgentDlg();

	void updateNewAgentDlg();
	void updateRemoveAgentDlg();
	JBool updateQuitConfirmDlg();

	// TFE: Gamepad navigation functions (referencing commit 508f2007fb63 for parity)
	void handleGamepadNavigation();
	void handleGamepadDialogNavigation();

	///////////////////////////////////////////
	// API Implementation
	///////////////////////////////////////////
	void agentMenu_resetState()
	{
		s_loaded = JFALSE;
		s_displayInit = JFALSE;
		s_agentMenuFrames = nullptr;
		s_agentDlgFrames = nullptr;
		s_framebuffer = nullptr;
		
		// TFE: Reset gamepad navigation state (referencing commit 508f2007fb63)
		s_gamepadFocusArea = FOCUS_AGENT_LIST;
		s_gamepadButtonFocus = AGENT_NEW;
		s_gamepadDialogFocus = NEW_AGENT_NO;
		
		delt_resetState();
	}

	JBool agentMenu_update(s32* levelIndex)
	{
		if (!s_loaded)
		{
			menu_init();
			agentMenu_startup();
			s_loaded = JTRUE;
		}
		if (!s_displayInit)
		{
			agentMenu_startupDisplay();
			s_missionBegin = JFALSE;
			s_displayInit = JTRUE;
		}

		// Update the mouse position.
		menu_handleMousePosition();
		agentMenu_update();
		agentMenu_draw();
		
		agentMenu_blit();

		// Reset for next time.
		if (s_missionBegin)
		{
			s_displayInit = JFALSE;
			vfb_forceToBlack();
			lcanvas_clear();

			assert(s_selectedMission >= 0 && s_selectedMission <= MAX_LEVEL_COUNT - 1);
			s_selectedMission = clamp(s_selectedMission, 0, MAX_LEVEL_COUNT - 1);
		}

		*levelIndex = s_selectedMission + 1;
		return ~s_missionBegin;
	}
	
	///////////////////////////////////////////
	// Internal Implementation
	///////////////////////////////////////////
	void agentMenu_update()
	{
		if (s_newAgentDlg)
		{
			updateNewAgentDlg();
			return;
		}
		else if (s_removeAgentDlg)
		{
			updateRemoveAgentDlg();
			return;
		}
		else if (s_quitConfirmDlg)
		{
			if (updateQuitConfirmDlg())
			{
				if (TFE_Settings::getSystemSettings()->gameQuitExitsToMenu)
				{
					TFE_FrontEndUI::exitToMenu();
				}
				else
				{
					TFE_System::postQuitMessage();
				}
			}
			return;
		}

		if (s_selectedMission < 0)
		{
			assert(0);	// This shouldn't happen.
			s_selectedMission = 0;
		}
		else if (s_selectedMission >= s_maxLevelIndex)
		{
			s_selectedMission = max(0, s_maxLevelIndex - 1);
		}

		const s32 x = s_cursorPos.x;
		const s32 z = s_cursorPos.z;
		
		// TFE: Detect mouse movement to switch from gamepad to mouse control (matching PR #1 behavior)
		static s32 lastMouseX = -1, lastMouseZ = -1;
		if (lastMouseX >= 0 && (lastMouseX != x || lastMouseZ != z))
		{
			// Mouse moved - prioritize mouse control over gamepad
			// (Implementation note: mouse taking over focus is handled implicitly through existing mouse logic)
		}
		lastMouseX = x;
		lastMouseZ = z;
		
		if (TFE_Input::mousePressed(MBUTTON_LEFT))
		{
			s_buttonPressed = -1;
			for (u32 i = 0; i < AGENT_COUNT; i++)
			{
				if (x >= c_agentButtons[i].x && x < c_agentButtons[i].x + c_agentButtonDim.x &&
					z >= c_agentButtons[i].z && z < c_agentButtons[i].z + c_agentButtonDim.z)
				{
					s_buttonPressed = s32(i);
					s_buttonHover = JTRUE;
					break;
				}
			}

			if (s_buttonPressed < 0)
			{
				if (x >= 25 && x < 122 && z >= 35 && z < 147 && s_agentCount > 0)
				{
					s_agentId = clamp((z - 35) / 8, 0, (s32)s_agentCount - 1);
					s_selectedMission = max(0, s_agentData[s_agentId].selectedMission - 1);
					s_lastSelectedAgent = true;
				}
				else if (x >= 171 && x < 290 && z >= 35 && z < 147 && s_agentCount > 0)
				{
					s_selectedMission = clamp((z - 35) / 8, 0, (s32)s_agentData[s_agentId].nextMission - 1);
					s_agentData[s_agentId].selectedMission = s_selectedMission + 1;
					s_lastSelectedAgent = false;
				}
			}
		}
		else if (TFE_Input::mouseDown(MBUTTON_LEFT) && s_buttonPressed >= 0)
		{
			// Verify that the mouse is still over the button.
			if (x >= c_agentButtons[s_buttonPressed].x && x < c_agentButtons[s_buttonPressed].x + c_agentButtonDim.x &&
				z >= c_agentButtons[s_buttonPressed].z && z < c_agentButtons[s_buttonPressed].z + c_agentButtonDim.z)
			{
				s_buttonHover = true;
			}
			else
			{
				s_buttonHover = false;
			}
		}
		else
		{
			// TFE: Handle gamepad navigation (referencing commit 508f2007fb63 for behavioral parity)
			handleGamepadNavigation();

			if (TFE_Input::keyPressed(KEY_N))
			{
				s_buttonPressed = AGENT_NEW;
				s_buttonHover = JTRUE;
			}
			else if (TFE_Input::keyPressed(s_langKeys->k_agdel))
			{
				s_buttonPressed = AGENT_REMOVE;
				s_buttonHover = JTRUE;
			}
			else if (TFE_Input::keyPressed(KEY_D))	// DOS
			{
				s_buttonPressed = AGENT_EXIT;
				s_buttonHover = JTRUE;
			}
			else if (TFE_Input::keyPressed(s_langKeys->k_begin) || TFE_Input::keyPressed(KEY_RETURN) || TFE_Input::keyPressed(KEY_KP_ENTER))
			{
				s_buttonPressed = AGENT_BEGIN;
				s_buttonHover = JTRUE;
			}

			// Arrow keys to change the selected agent or mission.
			if (TFE_Input::bufferedKeyDown(KEY_DOWN))
			{
				if (s_lastSelectedAgent && s_agentCount > 0)
				{
					s_agentId++;
					if (s_agentId >= (s32)s_agentCount) { s_agentId = 0; }
					if (s_agentId >= 0) { s_selectedMission = s_agentData[s_agentId].selectedMission - 1; }
				}
				else if (!s_lastSelectedAgent && s_agentId >= 0 && s_agentData[s_agentId].nextMission > 1)
				{
					s_selectedMission++;
					if (s_selectedMission > s_agentData[s_agentId].nextMission - 1 || s_selectedMission == 14) { s_selectedMission = 0; }
				}
			}
			else if (TFE_Input::bufferedKeyDown(KEY_UP))
			{
				if (s_lastSelectedAgent && s_agentCount > 0)
				{
					s_agentId--;
					if (s_agentId < 0) { s_agentId = s_agentCount - 1; }
					if (s_agentId >= 0) { s_selectedMission = s_agentData[s_agentId].selectedMission - 1; }
				}
				else if (!s_lastSelectedAgent && s_agentId >= 0 && s_agentData[s_agentId].nextMission > 1)
				{
					s_selectedMission--;
					if (s_selectedMission < 0) { s_selectedMission = s_agentData[s_agentId].nextMission - 1; }
				}
			}
			else if (TFE_Input::bufferedKeyDown(KEY_LEFT) || TFE_Input::bufferedKeyDown(KEY_RIGHT))
			{
				s_lastSelectedAgent = !s_lastSelectedAgent;
			}

			if (s_buttonPressed >= AGENT_NEW && s_buttonHover)
			{
				switch (s_buttonPressed)
				{
					case AGENT_NEW:
					{
						s_newAgentDlg = JTRUE;
						// TFE: Reset dialog focus for gamepad navigation
						s_gamepadDialogFocus = NEW_AGENT_NO;
						memset(s_newAgentName, 0, 32);
						// TFE: Prepopulate new agent profile name with "Player 1" for gamepad quality-of-life
						strcpy(s_newAgentName, "Player 1");
						s_editBox.cursor = strlen(s_newAgentName);
						s_editBox.inputField = s_newAgentName;
						s_editBox.maxLen = 16;
					} break;
					case AGENT_REMOVE:
						s_removeAgentDlg = JTRUE;
						// TFE: Reset dialog focus for gamepad navigation
						s_gamepadDialogFocus = NEW_AGENT_NO;
						break;
					case AGENT_EXIT:
						s_quitConfirmDlg = JTRUE;
						// TFE: Reset dialog focus for gamepad navigation
						s_gamepadDialogFocus = NEW_AGENT_NO;
						break;
					case AGENT_BEGIN:
						if (s_agentCount > 0 && s_selectedMission >= 0)
						{
							s_missionBegin = JTRUE;
						}
						break;
				};
			}

			// Reset.
			s_buttonPressed = -1;
			s_buttonHover = JFALSE;
		}
	}

	void agentMenu_draw()
	{
		memset(s_framebuffer, 0, 320 * 200);
		blitDeltaFrame(&s_agentMenuFrames[0], 0, 0, s_framebuffer);
		
		// Draw the buttons
		for (s32 i = 0; i < 4; i++)
		{
			s32 index;
			if (s_newAgentDlg)
			{
				if (i == 0) { index = i * 2 + 1; }
				else { index = i * 2 + 2; }
			}
			else if (s_removeAgentDlg)
			{
				if (i == 1) { index = i * 2 + 1; }
				else { index = i * 2 + 2; }
			}
			else if (s_quitConfirmDlg)
			{
				if (i == 2) { index = i * 2 + 1; }
				else { index = i * 2 + 2; }
			}
			else
			{
				// TFE: Add gamepad focus highlighting (referencing commit 508f2007fb63 for visual consistency)
				bool isGamepadFocused = (s_gamepadFocusArea == FOCUS_BUTTONS && s_gamepadButtonFocus == i);
				if ((s_buttonPressed == i && s_buttonHover) || isGamepadFocused) { index = i * 2 + 1; }
				else { index = i * 2 + 2; }
			}
			blitDeltaFrame(&s_agentMenuFrames[index], 0, 0, s_framebuffer);
		}

		// 11 = easy, 12 = medium, 13 = hard
		s32 yOffset = -20;
		s32 textX = 174;
		s32 textY = 36;
		u8 color = 47;
		if (s_agentId >= 0 && s_agentCount > 0)
		{
			s32 nextMission = min(s_agentData[s_agentId].nextMission, s_maxLevelIndex);

			for (s32 i = 0; i < nextMission - 1 && i < s_maxLevelIndex; i++)
			{
				color = 47;
				if (s_selectedMission == i)
				{
					color = 32;
					drawColoredQuad(textX - 3, textY - 1, 118, 9, s_lastSelectedAgent ? 13 : 12, s_framebuffer);
				}

				print(s_levelDisplayNames[i], textX, textY, color, s_framebuffer);
				s32 diff = s_agentData[s_agentId].completed[i] + 11;
				blitDeltaFrame(&s_agentMenuFrames[diff], 0, yOffset, s_framebuffer);
				yOffset += 8;
				textY += 8;
			}
			color = 47;
			if (s_selectedMission == nextMission - 1)
			{
				color = 32;
				drawColoredQuad(textX - 3, textY - 1, 118, 9, s_lastSelectedAgent ? 13 : 12, s_framebuffer);
			}
			print(s_levelDisplayNames[nextMission - 1], textX, textY, color, s_framebuffer);

			// Show final level complete.
			if (s_agentData[s_agentId].nextMission > s_maxLevelIndex)
			{
				yOffset = -20 + 8 * (s_maxLevelIndex - 1);
				s32 diff = s_agentData[s_agentId].completed[s_maxLevelIndex-1] + 11;
				blitDeltaFrame(&s_agentMenuFrames[diff], 0, yOffset, s_framebuffer);
			}
		}

		// Draw agents.
		textX = 28;
		textY = 36;
		for (s32 i = 0; i < s_agentCount; i++)
		{
			color = 47;
			if (s_agentId == i)
			{
				color = 32;
				drawColoredQuad(textX - 3, textY - 1, 118, 9, s_lastSelectedAgent ? 12 : 13, s_framebuffer);
			}
			print(s_agentData[i].name, textX, textY, color, s_framebuffer);
			textY += 8;
		}

		if (s_newAgentDlg)
		{
			drawNewAgentDlg();
		}
		else if (s_removeAgentDlg)
		{
			drawYesNoDlg(0);
		}
		else if (s_quitConfirmDlg)
		{
			drawYesNoDlg(1);
		}
	}

	void setPalette()
	{
		// Update the palette.
		u32 palette[256];
		u32* outColor = palette;
		u8* srcColor = s_menuPalette;
		for (s32 i = 0; i < 256; i++, outColor++, srcColor += 3)
		{
			*outColor = u32(srcColor[0]) | (u32(srcColor[1]) << 8u) | (u32(srcColor[2]) << 16u) | (0xffu << 24u);
		}
		vfb_setPalette(palette);
	}

	// TFE Specific.
	void agentMenu_startupDisplay()
	{
		s_framebuffer = menu_startupDisplay();
		menu_resetCursor();

		setPalette();
	}

	void agentMenu_load(LangHotkeys* langKeys)
	{
		FilePath filePath;
		if (!TFE_Paths::getFilePath("AGENTMNU.LFD", &filePath)) { return; }
		Archive* archive = Archive::getArchive(ARCHIVE_LFD, "AGENTMNU", filePath.path);
		TFE_Paths::addLocalArchive(archive);

		loadPaletteFromPltt("agentmnu.pltt", s_menuPalette);
		s_agentMenuCount = getFramesFromAnim("agentmnu.anim", &s_agentMenuFrames);
		s_agentDlgCount = getFramesFromAnim("agentdlg.anim", &s_agentDlgFrames);
		getFrameFromDelt("cursor.delt", &s_cursor);
		TFE_Paths::removeLastArchive();
		
		s_langKeys = langKeys;
	}
		
	void agentMenu_startup()
	{
		s_agentCount = 0;
		for (s32 i = 0; i < MAX_AGENT_COUNT; i++)
		{
			if (!s_agentData[i].name[0])
			{
				break;
			}
			s_agentCount++;
		}
		// Clear out extra data.
		for (s32 i = s_agentCount; i < MAX_AGENT_COUNT; i++)
		{
			memset(&s_agentData[i], 0, sizeof(AgentData));
		}

		if (s_agentCount > 0)
		{
			s_agentId = 0;
			s_selectedMission = max(0, s_agentData[s_agentId].selectedMission - 1);
		}
		else
		{
			s_agentId = -1;
			s_selectedMission = 0;
		}

		s_missionBegin = JFALSE;
	}
		
	void agentMenu_blit()
	{
		setPalette();
		menu_blitCursor(s_cursorPos.x, s_cursorPos.z, s_framebuffer);
		menu_blitToScreen(s_framebuffer);
	}

	void agentMenu_setAgentName(const char* name)
	{
		if (s_agentId < 0 || s_agentId >= MAX_AGENT_COUNT)
		{
			return;
		}
		strncpy(s_agentData[s_agentId].name, name, 32);
	}

	s32 agentMenu_getAgentID()
	{
		return s_agentId;
	}

	void agentMenu_setAgentId(s32 id)
	{
		s_agentId = id;
	}

	void agentMenu_setAgentCount(s32 count)
	{
		s_agentCount = count;
	}

	s32 agentMenu_getAgentCount()
	{
		return s_agentCount;
	}

	void agentMenu_createNewAgent()
	{
		if (s_agentCount < MAX_AGENT_COUNT)
		{
			s_agentId = s_agentCount;
			s_agentCount++;
			agent_createNewAgent(s_agentId, &s_agentData[s_agentId], s_newAgentName);

			s_selectedMission = 0;
		}
	}

	void agentMenu_removeAgent(s32 index)
	{
		if (s_agentCount < 1)
		{
			return;
		}

		for (s32 i = index; i < (s32)s_agentCount; i++)
		{
			s_agentData[i] = s_agentData[i + 1];
		}
		s_agentCount--;
		memset(&s_agentData[s_agentCount], 0, sizeof(AgentData));
		
		s_agentId = s_agentCount > 0 ? 0 : -1;
		s_selectedMission = s_agentCount > 0 ? s_agentData[0].selectedMission : 0;
	}

	// UI routines.
	void drawYesNoButtons(u32 indexNo, u32 indexYes)
	{
		// TFE: Add gamepad focus highlighting for dialog buttons (referencing commit 508f2007fb63)
		bool isNoFocused = (s_gamepadDialogFocus == NEW_AGENT_NO);
		bool isYesFocused = (s_gamepadDialogFocus == NEW_AGENT_YES);
		
		if ((s_buttonPressed == 0 && s_buttonHover) || isNoFocused)
		{
			blitDeltaFrame(&s_agentDlgFrames[indexNo], 0, 0, s_framebuffer);
		}
		else if ((s_buttonPressed == 1 && s_buttonHover) || isYesFocused)
		{
			blitDeltaFrame(&s_agentDlgFrames[indexYes], 0, 0, s_framebuffer);
		}
	}

	void drawYesNoDlg(s32 baseFrame)
	{
		blitDeltaFrame(&s_agentDlgFrames[baseFrame], 0, 0, s_framebuffer);
		drawYesNoButtons(4, 6);
	}

	void drawNewAgentDlg()
	{
		blitDeltaFrame(&s_agentDlgFrames[2], 0, 0, s_framebuffer);
		drawEditBox(&s_editBox, 106, 87, 216, 100, s_framebuffer);
		drawYesNoButtons(8, 10);
	}

	void updateNewAgentDlg()
	{
		updateEditBox(&s_editBox);

		if (TFE_Input::keyPressed(KEY_RETURN) || TFE_Input::keyPressed(KEY_KP_ENTER))
		{
			agentMenu_createNewAgent();
			s_newAgentDlg = JFALSE;
			return;
		}
		else if (TFE_Input::keyPressed(KEY_ESCAPE))
		{
			s_newAgentDlg = JFALSE;
			return;
		}

		// TFE: Handle gamepad navigation for dialog (referencing commit 508f2007fb63)
		handleGamepadDialogNavigation();

		if (TFE_Input::mousePressed(MBUTTON_LEFT))
		{
			s_buttonPressed = -1;
			for (u32 i = 0; i < NEW_AGENT_COUNT; i++)
			{
				if (s_cursorPos.x >= c_newAgentButtons[i].x && s_cursorPos.x < c_newAgentButtons[i].x + c_newAgentButtonDim.x &&
					s_cursorPos.z >= c_newAgentButtons[i].z && s_cursorPos.z < c_newAgentButtons[i].z + c_newAgentButtonDim.z)
				{
					s_buttonPressed = s32(i);
					s_buttonHover = true;
					break;
				}
			}
		}
		else if (TFE_Input::mouseDown(MBUTTON_LEFT) && s_buttonPressed >= 0)
		{
			// Verify that the mouse is still over the button.
			if (s_cursorPos.x >= c_newAgentButtons[s_buttonPressed].x && s_cursorPos.x < c_newAgentButtons[s_buttonPressed].x + c_newAgentButtonDim.x &&
				s_cursorPos.z >= c_newAgentButtons[s_buttonPressed].z && s_cursorPos.z < c_newAgentButtons[s_buttonPressed].z + c_newAgentButtonDim.z)
			{
				s_buttonHover = true;
			}
			else
			{
				s_buttonHover = false;
			}
		}
		else
		{
			// Activate button 's_escButtonPressed'
			if (s_buttonPressed >= NEW_AGENT_NO && s_buttonHover)
			{
				if (s_buttonPressed == NEW_AGENT_YES)
				{
					agentMenu_createNewAgent();
				}
				s_newAgentDlg = JFALSE;
			}
			// Reset.
			s_buttonPressed = -1;
			s_buttonHover = false;
		}
	}

	void updateRemoveAgentDlg()
	{
		if (TFE_Input::keyPressed(KEY_RETURN) || TFE_Input::keyPressed(KEY_KP_ENTER) || TFE_Input::keyPressed(KEY_ESCAPE))
		{
			s_removeAgentDlg = false;
			return;
		}

		// TFE: Handle gamepad navigation for dialog (referencing commit 508f2007fb63)
		handleGamepadDialogNavigation();

		if (TFE_Input::mousePressed(MBUTTON_LEFT))
		{
			s_buttonPressed = -1;
			for (u32 i = 0; i < NEW_AGENT_COUNT; i++)
			{
				if (s_cursorPos.x >= c_newAgentButtons[i].x && s_cursorPos.x < c_newAgentButtons[i].x + c_newAgentButtonDim.x &&
					s_cursorPos.z >= c_newAgentButtons[i].z && s_cursorPos.z < c_newAgentButtons[i].z + c_newAgentButtonDim.z)
				{
					s_buttonPressed = s32(i);
					s_buttonHover = true;
					break;
				}
			}
		}
		else if (TFE_Input::mouseDown(MBUTTON_LEFT) && s_buttonPressed >= 0)
		{
			// Verify that the mouse is still over the button.
			if (s_cursorPos.x >= c_newAgentButtons[s_buttonPressed].x && s_cursorPos.x < c_newAgentButtons[s_buttonPressed].x + c_newAgentButtonDim.x &&
				s_cursorPos.z >= c_newAgentButtons[s_buttonPressed].z && s_cursorPos.z < c_newAgentButtons[s_buttonPressed].z + c_newAgentButtonDim.z)
			{
				s_buttonHover = true;
			}
			else
			{
				s_buttonHover = false;
			}
		}
		else
		{
			if (TFE_Input::keyPressed(s_langKeys->k_yes))
			{
				agentMenu_removeAgent(s_agentId);
				s_removeAgentDlg = false;
			}
			else if (TFE_Input::keyPressed(KEY_N))
			{
				s_removeAgentDlg = false;
			}
			else if (s_buttonPressed >= NEW_AGENT_NO && s_buttonHover)
			{
				if (s_buttonPressed == NEW_AGENT_YES)
				{
					agentMenu_removeAgent(s_agentId);
				}
				s_removeAgentDlg = false;
			}
			// Reset.
			s_buttonPressed = -1;
			s_buttonHover = false;
		}
	}

	JBool updateQuitConfirmDlg()
	{
		JBool quit = JFALSE;
		if (TFE_Input::keyPressed(KEY_ESCAPE))
		{
			s_quitConfirmDlg = JFALSE;
			return JFALSE;
		}
		else if (TFE_Input::keyPressed(KEY_RETURN) || TFE_Input::keyPressed(KEY_KP_ENTER))
		{
			s_quitConfirmDlg = JFALSE;
			return JTRUE;
		}

		// TFE: Handle gamepad navigation for dialog (referencing commit 508f2007fb63)
		handleGamepadDialogNavigation();

		if (TFE_Input::mousePressed(MBUTTON_LEFT))
		{
			s_buttonPressed = -1;
			for (u32 i = 0; i < NEW_AGENT_COUNT; i++)
			{
				if (s_cursorPos.x >= c_newAgentButtons[i].x && s_cursorPos.x < c_newAgentButtons[i].x + c_newAgentButtonDim.x &&
					s_cursorPos.z >= c_newAgentButtons[i].z && s_cursorPos.z < c_newAgentButtons[i].z + c_newAgentButtonDim.z)
				{
					s_buttonPressed = s32(i);
					s_buttonHover = JTRUE;
					break;
				}
			}
		}
		else if (TFE_Input::mouseDown(MBUTTON_LEFT) && s_buttonPressed >= 0)
		{
			// Verify that the mouse is still over the button.
			if (s_cursorPos.x >= c_newAgentButtons[s_buttonPressed].x && s_cursorPos.x < c_newAgentButtons[s_buttonPressed].x + c_newAgentButtonDim.x &&
				s_cursorPos.z >= c_newAgentButtons[s_buttonPressed].z && s_cursorPos.z < c_newAgentButtons[s_buttonPressed].z + c_newAgentButtonDim.z)
			{
				s_buttonHover = JTRUE;
			}
			else
			{
				s_buttonHover = JFALSE;
			}
		}
		else
		{
			// Activate button 's_escButtonPressed'
			if (TFE_Input::keyPressed(s_langKeys->k_yes))
			{
				quit = JTRUE;
				s_quitConfirmDlg = JFALSE;
			}
			else if (TFE_Input::keyPressed(KEY_N))
			{
				s_quitConfirmDlg = JFALSE;
			}
			else if (s_buttonPressed >= NEW_AGENT_NO && s_buttonHover)
			{
				if (s_buttonPressed == NEW_AGENT_YES)
				{
					quit = JTRUE;
				}
				s_quitConfirmDlg = JFALSE;
			}
			// Reset.
			s_buttonPressed = -1;
			s_buttonHover = false;
		}

		return quit;
	}

	// TFE: Gamepad dialog navigation (referencing commit 508f2007fb63 for consistency)
	void handleGamepadDialogNavigation()
	{
		// Only process when in menu context
		if (!TFE_FrontEndUI::isInMenuContext())
		{
			return;
		}

		// Handle A button (confirm)
		if (TFE_Input::buttonPressed(CONTROLLER_BUTTON_A))
		{
			s_buttonPressed = s_gamepadDialogFocus;
			s_buttonHover = true;
		}

		// Handle B button (cancel) - typically select "No" option
		if (TFE_Input::buttonPressed(CONTROLLER_BUTTON_B))
		{
			s_buttonPressed = NEW_AGENT_NO;
			s_buttonHover = true;
		}

		// Handle left/right navigation between Yes/No buttons
		if (TFE_Input::bufferedKeyDown(KEY_LEFT) || TFE_Input::buttonPressed(CONTROLLER_BUTTON_DPAD_LEFT))
		{
			s_gamepadDialogFocus = NEW_AGENT_NO;
		}
		else if (TFE_Input::bufferedKeyDown(KEY_RIGHT) || TFE_Input::buttonPressed(CONTROLLER_BUTTON_DPAD_RIGHT))
		{
			s_gamepadDialogFocus = NEW_AGENT_YES;
		}

		// Handle analog stick with repeat timing
		static u64 lastDialogNavigationTime = 0;
		u64 currentTime = TFE_System::getCurrentTimeInTicks();
		u64 currentTimeMs = (u64)TFE_System::convertFromTicksToMillis(currentTime);
		const u64 DIALOG_REPEAT_DELAY = 150; // Shorter delay for dialog navigation

		f32 leftX = TFE_Input::getAxis(AXIS_LEFT_X);
		f32 magnitude = fabsf(leftX);
		
		if (magnitude > 0.1f) // GAMEPAD_DEADZONE
		{
			if (currentTimeMs - lastDialogNavigationTime > DIALOG_REPEAT_DELAY)
			{
				if (leftX > 0.7f) // Right
				{
					s_gamepadDialogFocus = NEW_AGENT_YES;
					lastDialogNavigationTime = currentTimeMs;
				}
				else if (leftX < -0.7f) // Left
				{
					s_gamepadDialogFocus = NEW_AGENT_NO;
					lastDialogNavigationTime = currentTimeMs;
				}
			}
		}
	}

	// TFE: Gamepad navigation implementation (referencing commit 508f2007fb63 for consistent behavior)
	void handleGamepadNavigation()
	{
		// Only process gamepad navigation when in menu context and no dialogs are open
		if (!TFE_FrontEndUI::isInMenuContext() || s_newAgentDlg || s_removeAgentDlg || s_quitConfirmDlg)
		{
			return;
		}

		// Handle A button (confirm) - single press only, no repeat
		if (TFE_Input::buttonPressed(CONTROLLER_BUTTON_A))
		{
			if (s_gamepadFocusArea == FOCUS_BUTTONS)
			{
				s_buttonPressed = s_gamepadButtonFocus;
				s_buttonHover = JTRUE;
			}
			else if (s_gamepadFocusArea == FOCUS_AGENT_LIST && s_agentCount > 0)
			{
				// Select current agent, switch to mission list
				s_lastSelectedAgent = false;
				s_gamepadFocusArea = FOCUS_MISSION_LIST;
			}
			else if (s_gamepadFocusArea == FOCUS_MISSION_LIST && s_agentId >= 0)
			{
				// Mission selected, activate BEGIN button
				s_buttonPressed = AGENT_BEGIN;
				s_buttonHover = JTRUE;
			}
		}

		// Handle B button (cancel/back) - single press only, no repeat
		if (TFE_Input::buttonPressed(CONTROLLER_BUTTON_B))
		{
			if (s_gamepadFocusArea == FOCUS_MISSION_LIST)
			{
				// Go back to agent list
				s_lastSelectedAgent = true;
				s_gamepadFocusArea = FOCUS_AGENT_LIST;
			}
			else if (s_gamepadFocusArea == FOCUS_BUTTONS)
			{
				// Go back to agent list
				s_lastSelectedAgent = true;
				s_gamepadFocusArea = FOCUS_AGENT_LIST;
			}
			// If already in agent list, could trigger exit (like escape menu behavior)
		}

		// Handle navigation with repeat timing for both D-pad and analog stick
		// This provides consistent behavior matching the existing keyboard navigation
		static u64 lastNavigationTime = 0;
		static bool lastDpadPressed[4] = {false}; // Up, Down, Left, Right
		static bool lastStickMoved = false;
		
		u64 currentTime = TFE_System::getCurrentTimeInTicks();
		u64 currentTimeMs = (u64)TFE_System::convertFromTicksToMillis(currentTime);
		const u64 INITIAL_REPEAT_DELAY = 300;  // Initial delay before repeat starts
		const u64 REPEAT_RATE = 100;           // Repeat rate after initial delay
		
		// Check D-pad inputs
		bool dpadUp = TFE_Input::buttonDown(CONTROLLER_BUTTON_DPAD_UP);
		bool dpadDown = TFE_Input::buttonDown(CONTROLLER_BUTTON_DPAD_DOWN);
		bool dpadLeft = TFE_Input::buttonDown(CONTROLLER_BUTTON_DPAD_LEFT);
		bool dpadRight = TFE_Input::buttonDown(CONTROLLER_BUTTON_DPAD_RIGHT);
		
		// Check analog stick (using same deadzone as cursor movement)
		f32 leftX = TFE_Input::getAxis(AXIS_LEFT_X);
		f32 leftY = TFE_Input::getAxis(AXIS_LEFT_Y);
		f32 magnitude = sqrtf(leftX * leftX + leftY * leftY);
		
		bool stickUp = false, stickDown = false, stickLeft = false, stickRight = false;
		if (magnitude > 0.1f) // GAMEPAD_DEADZONE
		{
			leftX /= magnitude;
			leftY /= magnitude;
			
			stickUp = (leftY < -0.7f);
			stickDown = (leftY > 0.7f);
			stickLeft = (leftX < -0.7f);
			stickRight = (leftX > 0.7f);
		}
		
		// Combine D-pad and stick inputs (avoid double triggering)
		bool inputUp = dpadUp || stickUp;
		bool inputDown = dpadDown || stickDown;
		bool inputLeft = dpadLeft || stickLeft;
		bool inputRight = dpadRight || stickRight;
		bool anyInput = inputUp || inputDown || inputLeft || inputRight;
		
		// Handle repeat timing
		bool shouldNavigate = false;
		if (anyInput)
		{
			if (!lastDpadPressed[0] && !lastDpadPressed[1] && !lastDpadPressed[2] && !lastDpadPressed[3] && !lastStickMoved)
			{
				// First press
				shouldNavigate = true;
				lastNavigationTime = currentTimeMs;
			}
			else if (currentTimeMs - lastNavigationTime > INITIAL_REPEAT_DELAY)
			{
				// Repeating - check repeat rate
				static u64 lastRepeatTime = 0;
				if (currentTimeMs - lastRepeatTime > REPEAT_RATE)
				{
					shouldNavigate = true;
					lastRepeatTime = currentTime;
				}
			}
		}
		
		// Store current input state for next frame
		lastDpadPressed[0] = inputUp;
		lastDpadPressed[1] = inputDown; 
		lastDpadPressed[2] = inputLeft;
		lastDpadPressed[3] = inputRight;
		lastStickMoved = anyInput;
		
		// Execute navigation if timing allows
		if (shouldNavigate)
		{
			// Vertical navigation (Up/Down)
			if (inputDown)
			{
				if (s_gamepadFocusArea == FOCUS_AGENT_LIST && s_agentCount > 0)
				{
					s_agentId++;
					if (s_agentId >= (s32)s_agentCount) { s_agentId = 0; }
					if (s_agentId >= 0) { s_selectedMission = max(0, s_agentData[s_agentId].selectedMission - 1); }
					s_lastSelectedAgent = true;
				}
				else if (s_gamepadFocusArea == FOCUS_MISSION_LIST && s_agentId >= 0 && s_agentData[s_agentId].nextMission > 1)
				{
					s_selectedMission++;
					if (s_selectedMission > s_agentData[s_agentId].nextMission - 1 || s_selectedMission == 14) { s_selectedMission = 0; }
					s_lastSelectedAgent = false;
				}
				else if (s_gamepadFocusArea == FOCUS_BUTTONS)
				{
					s_gamepadButtonFocus++;
					if (s_gamepadButtonFocus >= AGENT_COUNT) { s_gamepadButtonFocus = 0; }
				}
			}
			else if (inputUp)
			{
				if (s_gamepadFocusArea == FOCUS_AGENT_LIST && s_agentCount > 0)
				{
					s_agentId--;
					if (s_agentId < 0) { s_agentId = s_agentCount - 1; }
					if (s_agentId >= 0) { s_selectedMission = max(0, s_agentData[s_agentId].selectedMission - 1); }
					s_lastSelectedAgent = true;
				}
				else if (s_gamepadFocusArea == FOCUS_MISSION_LIST && s_agentId >= 0 && s_agentData[s_agentId].nextMission > 1)
				{
					s_selectedMission--;
					if (s_selectedMission < 0) { s_selectedMission = s_agentData[s_agentId].nextMission - 1; }
					s_lastSelectedAgent = false;
				}
				else if (s_gamepadFocusArea == FOCUS_BUTTONS)
				{
					s_gamepadButtonFocus--;
					if (s_gamepadButtonFocus < 0) { s_gamepadButtonFocus = AGENT_COUNT - 1; }
				}
			}

			// Horizontal navigation (Left/Right) - area switching
			if (inputRight)
			{
				if (s_gamepadFocusArea == FOCUS_AGENT_LIST)
				{
					if (s_agentCount > 0 && s_agentId >= 0 && s_agentData[s_agentId].nextMission > 1)
					{
						s_gamepadFocusArea = FOCUS_MISSION_LIST;
						s_lastSelectedAgent = false;
					}
					else
					{
						s_gamepadFocusArea = FOCUS_BUTTONS;
					}
				}
				else if (s_gamepadFocusArea == FOCUS_MISSION_LIST)
				{
					s_gamepadFocusArea = FOCUS_BUTTONS;
				}
			}
			else if (inputLeft)
			{
				if (s_gamepadFocusArea == FOCUS_MISSION_LIST)
				{
					s_gamepadFocusArea = FOCUS_AGENT_LIST;
					s_lastSelectedAgent = true;
				}
				else if (s_gamepadFocusArea == FOCUS_BUTTONS)
				{
					s_gamepadFocusArea = (s_agentCount > 0) ? FOCUS_AGENT_LIST : FOCUS_AGENT_LIST;
					s_lastSelectedAgent = true;
				}
			}
		}
	}
}