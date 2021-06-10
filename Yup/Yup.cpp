#include "nvse/PluginAPI.h"
#include "nvse/GameData.h"
#include "nvse/GameForms.h"
#include "nvse/GameObjects.h"
#include "nvse/SafeWrite.h"
#include "nvse/Utilities.h"
#include "nvse/GameTiles.h"
#include "params.h"

// rand code taken from https://www.cplusplus.com/reference/cstdlib/srand/
#include <cstdlib>     /* srand, rand */
#include <ctime>       /* time */

 
// Plugin Stuff
IDebugLog g_Log("YupNVSE.log");
UInt32 g_MyPluginVersion = 100;
const char* g_MyPluginName = "Yup NVSE";


TESActorBaseData::FactionListData easyPeteFaction;
const auto easyPeteForm = (TESNPC*)LookupFormByID(0x00104C7F);
const auto dynamiteForm = LookupFormByID(0x00109A0C);

__forceinline void TESNPC::CopyAppearance(TESNPC* srcNPC)  //thanks to c6 for pointing this out
{
	ThisStdCall(0x603790, this, srcNPC);
}

// Credit goes to Jazzisparis for the "BaseAddItem" code (from JIPLN).
__forceinline void BaseAddItem(TESContainer* container, TESForm* item, UInt32 count)
{
	if (!container || !item || !count) return;

	ListNode<TESContainer::FormCount>* iter = container->formCountList.Head();
	TESContainer::FormCount* formCount;
	do
	{
		formCount = iter->data;
		if (formCount && (formCount->form == item))
		{
			formCount->count += count;
			return;
		}
	} 	while (iter = iter->next);
	formCount = (TESContainer::FormCount*)GameHeapAlloc(sizeof(TESContainer::FormCount));
	formCount->contExtraData = (ContainerExtraData*)GameHeapAlloc(sizeof(ContainerExtraData));
	formCount->form = item;
	formCount->count = count;
	formCount->contExtraData->itemCondition = 1.0;
	container->formCountList.Insert(formCount);
}

std::string voiceLineBasePath = "Data\\Sound\\Voice\\FalloutNV.esm\\MaleOld02\\";

std::vector<std::string> voiceLinesPaths = 
{
"VFreeformG_VFreeformGoodsp_00104C57_1",  // "yup"
"VFreeformG_VFreeformGoodsp_00104DED_1",  // "uh-huh"
"VFreeformG_VFreeformGoodsp_0015D3DE_1",  // "yup" 2
"VFreeformG_VFreeformGoodsp_00104DE8_1",  // "welcome"
"VFreeformG_VFreeformGoodsp_00104DEB_1",  // "bad trouble"
"VFreeformG_VFreeformGoodsp_00104C44_1",  // "too dangerous..."
"VFreeformG_VFreeformGoodsp_00104C53_1",  // "too dangerous..." 2
};




// This is a message handler for nvse events
// With this, plugins can listen to messages such as whenever the game loads
void MessageHandler(NVSEMessagingInterface::Message* msg)
{
	switch (msg->type)
	{

	case NVSEMessagingInterface::kMessage_DeferredInit:

		srand(time(NULL));

		UInt8 FNV = DataHandler::Get()->GetModIndex("FalloutNV.esm");
		for (auto iter = DataHandler::Get()->boundObjectList->first; iter; iter = iter->next)
		{
			if (iter->typeID == 42)  // NPC Type.
			{
				TESNPC* npc = (TESNPC*)iter;
				
				std::string name = "Easy Pete";
				npc->fullName.name.Set(name.c_str());
				
				npc->CopyAppearance(easyPeteForm);
				
				easyPeteFaction.faction = (TESFaction*)LookupFormByID(0x00104C6E);  // GoodspringsFaction
				npc->baseData.factionList.Append(&easyPeteFaction);
				easyPeteFaction.faction = (TESFaction*)LookupFormByID(0x0015EC58);  // GoodspringsMilitiaFaction
				npc->baseData.factionList.Append(&easyPeteFaction);
				
				npc->baseData.voiceType = (BGSVoiceType*)LookupFormByID(0x0013C8DB);  //MaleOld02

				TESContainer* container = DYNAMIC_CAST(npc, TESNPC, TESContainer);
				if (container)
				{
					// Generate random number from 1-10, add that many dynamites.
					BaseAddItem(container, dynamiteForm, (rand() % 10 + 1));
				}
			}
		}

		// Change voice lines
		//WriteRelCall(, (UInt32)Actor_DoSpeechLoadLipFiles_strcpy_s_Hook);   // strcpy_s_(Dst, 0x200u, Src);

		//todo: make lipsynch work

		
		break;
	}
}


//No idea why the extern "C" is there.
extern "C"
{

	bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info)
	{
		// fill out the info structure
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = g_MyPluginName;
		info->version = g_MyPluginVersion;

		// version checks
		if (nvse->nvseVersion < PACKED_NVSE_VERSION)  //fixed version check thanks to c6
		{
			char buffer[100];
			sprintf_s(buffer, 100,"NVSE version too old (got %08X expected at least %08X)", nvse->nvseVersion, PACKED_NVSE_VERSION);
			MessageBoxA(nullptr, buffer, g_MyPluginName, MB_ICONEXCLAMATION);  //MessageBoxA usage style copied from lStewieAl.
			_ERROR("%s", buffer);
			return false;
		}
		
		if (!nvse->isEditor)
		{
			if (nvse->runtimeVersion < RUNTIME_VERSION_1_4_0_525)
			{
				char buffer[100];
				sprintf_s(buffer, 100, "Incorrect runtime version (got %08X need at least %08X)", nvse->runtimeVersion, RUNTIME_VERSION_1_4_0_525);
				MessageBoxA(nullptr, buffer, g_MyPluginName, MB_ICONEXCLAMATION);
				_ERROR("%s", buffer);
				return false;
			}

			if (nvse->isNogore)
			{
				char buffer[] = "NoGore is not supported";
				MessageBoxA(nullptr, buffer, g_MyPluginName, MB_ICONEXCLAMATION);
				_ERROR("%s", buffer);
				return false;
			}
		}

		// version checks pass
		// any version compatibility checks should be done here
		return true;
	}


	bool NVSEPlugin_Load(const NVSEInterface* nvse)
	{
		if (!nvse->isEditor)
		{
			_MESSAGE("%s loaded successfully (In-Game).\nPlugin version: %u\n", g_MyPluginName, g_MyPluginVersion);
		}
		
		// register to receive messages from NVSE
		((NVSEMessagingInterface*)nvse->QueryInterface(kInterface_Messaging))->RegisterListener(nvse->GetPluginHandle(), "NVSE", MessageHandler);
		
		return true;
	}

};
