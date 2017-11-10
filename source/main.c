/*
ALttP Randomizer Generator
Copyright (C) 2017 Michael Ragusa

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <inttypes.h>

#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include "nxjson.h"

//This include a header containing definitions of our image
#include "brew_bgr.h"
#define NUM_OPTIONS 7
#define NUM_MODES 3
#define NUM_GOALS 4
#define NUM_LOGICS 3
#define NUM_DIFICULTIES 5
#define NUM_VARIATIONS 6
#define TWO_MB 2097152
#define MAX_QUERY_STRING 300
#define NUM_SPRITES 64

const char* g_modes[NUM_MODES];
const char* g_goals[NUM_GOALS];
const char* g_logics[NUM_LOGICS];
const char* g_dificulties[NUM_DIFICULTIES];
const char* g_variations[NUM_VARIATIONS];
const char* g_sprites[NUM_SPRITES];
char queryString[MAX_QUERY_STRING];
char seedFileName[120];

int g_curOptionIdx;

struct OptionState
{
	int m_curMode;
	int m_curGoal;
	int m_curLogic;
	int m_curDifficulty;
	int m_curVariation;
	int m_curSprite;
};

struct OptionState g_curState;

void prepStatus()
{
	// clear status line(s)
	for (int i = 21; i < 31; ++i)
	{
		for (int j = 0; j < 53; ++j)
		{
			printf("\x1b[%d;%dH ", i, j);
		}
	}
	// Set cursor
	printf("\x1b[21;0H");
}

// TODO: FIX ME
void updateChecksum(u8* rom)
{
	u16 sum = 0;
	for (int i = 0; i < TWO_MB; ++i)
	{
		if (i >= 0x7FB0 && i > 0x7FE0)
		{
			continue;
		}
		sum += rom[i];
	}
	
	u16 checksum = sum & 0xFFFF;
	u16 inverse = checksum ^ 0xFFFF;
	
	u16 offset = 0x7FDC;
	memcpy(rom + offset, &inverse, 2);
	memcpy(rom + offset + 2, &checksum, 2);
}

void parseJSON(char* buf, const char* fileToLoad, const char* fileToPatch)
{
	const nx_json* json = nx_json_parse(buf, 0);
	if (!json)
	{
		prepStatus();
		printf("JSON parse failed\n");
		return;
	}
	
	FILE* file = fopen(fileToLoad, "rb");
	if (file == NULL)
	{
		prepStatus();
		printf("%s could not be loaded\n", fileToLoad);
		return;
	}
	prepStatus();
	printf("Reading file %s\n", fileToLoad);
	
	// Get current rom size
	fseek(file, 0L, SEEK_END);
	u32 size = ftell(file);
	rewind(file);
	
	// Copy it into memory
	u8* rom = (u8*)calloc(TWO_MB, sizeof(u8));
	fread(rom, size, 1, file);
	fclose(file);
	
	
	
	const nx_json* patchArr = nx_json_get(json, "patch");
	memset(&seedFileName[0], 0, sizeof(seedFileName));
	
	bool original = patchArr->type == NX_JSON_NULL;
	if (original)
	{
		// This is the original patch, grab the root
		patchArr = json;
	}
	
	for (int i = 0; i < patchArr->length; ++i)
	{
		const nx_json* patchObj = nx_json_item(patchArr, i);
		const nx_json* seekKeyObj = nx_json_item(patchObj, 0);
		int seek = atoi(seekKeyObj->key);
		const nx_json* patchValuesArr = nx_json_get(patchObj, seekKeyObj->key);
		if (patchValuesArr->type == NX_JSON_INTEGER)
		{
			rom[seek] = patchValuesArr->int_value;
		}
		else if (patchValuesArr->type == NX_JSON_ARRAY)
		{
			for (int j = 0; j < patchValuesArr->length; ++j)
			{
				u8 patchValue = nx_json_item(patchValuesArr, j)->int_value;
				rom[seek] = patchValue;
				++seek;
			}
		}
	}
	
	//TODO: FIXME
	//updateChecksum(rom);
	
	if (original)
	{
		strcpy(&seedFileName[0], fileToPatch);
	}
	else
	{
		const nx_json* spoiler = nx_json_get(json, "spoiler");
		const nx_json* meta = nx_json_get(spoiler, "meta");
		const nx_json* jdifficulty = nx_json_get(meta, "difficulty");
		//const nx_json* jvariation = nx_json_get(meta, "variation");
		const nx_json* jlogic = nx_json_get(meta, "logic");
		const nx_json* jseed = nx_json_get(meta, "seed");
		const nx_json* jgoal = nx_json_get(meta, "goal");
		const nx_json* jmode = nx_json_get(meta, "mode");
		sprintf(&seedFileName[0], "ALttP - VT_%s_%s-%s-%s_%d.sfc", 
			jlogic->text_value,
			jdifficulty->text_value,
			jmode->text_value,
			jgoal->text_value,
			(int)jseed->int_value);
	}	
			
	
	
	FILE* writeFile;
	writeFile = fopen(seedFileName, "wb");
	fwrite(rom, TWO_MB, 1, writeFile);	
	fclose(writeFile);
	free(rom);
	
	prepStatus();
	printf("Wrote %s\n", seedFileName);
}

u32 http_download(const char *url, u8** outputBuf)
{
	Result ret=0;
	httpcContext context;
	char *newurl=NULL;
	u32 statuscode=0;
	u32 contentsize=0, readsize=0, size=0;
	u8 *buf, *lastbuf;

	printf("Downloading %s\n",url);

	do {
		ret = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 1);
		//printf("return from httpcOpenContext: %" PRId32 "\n",ret);

		// This disables SSL cert verification, so https:// will be usable
		ret = httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);
		//printf("return from httpcSetSSLOpt: %" PRId32 "\n",ret);

		// Enable Keep-Alive connections
		ret = httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED);
		//printf("return from httpcSetKeepAlive: %" PRId32 "\n",ret);

		// Set a User-Agent header so websites can identify your application
		ret = httpcAddRequestHeaderField(&context, "User-Agent", "httpc-example/1.0.0");
		//printf("return from httpcAddRequestHeaderField: %" PRId32 "\n",ret);

		// Tell the server we can support Keep-Alive connections.
		// This will delay connection teardown momentarily (typically 5s)
		// in case there is another request made to the same server.
		ret = httpcAddRequestHeaderField(&context, "Connection", "Keep-Alive");
		//printf("return from httpcAddRequestHeaderField: %" PRId32 "\n",ret);

		ret = httpcBeginRequest(&context);
		if(ret!=0){
			httpcCloseContext(&context);
			if(newurl!=NULL) free(newurl);
			return ret;
		}

		ret = httpcGetResponseStatusCode(&context, &statuscode);
		if(ret!=0){
			httpcCloseContext(&context);
			if(newurl!=NULL) free(newurl);
			return ret;
		}

		if ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308)) {
			if(newurl==NULL) newurl = (char*)malloc(0x1000); // One 4K page for new URL
			if (newurl==NULL){
				httpcCloseContext(&context);
				return -1;
			}
			ret = httpcGetResponseHeader(&context, "Location", newurl, 0x1000);
			url = newurl; // Change pointer to the url that we just learned
			//printf("redirecting to url: %s\n",url);
			httpcCloseContext(&context); // Close this context before we try the next
		}
	} while ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308));

	if(statuscode!=200){
		//printf("URL returned status: %" PRId32 "\n", statuscode);
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return -2;
	}

	// This relies on an optional Content-Length header and may be 0
	ret=httpcGetDownloadSizeState(&context, NULL, &contentsize);
	if(ret!=0){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return ret;
	}

	//printf("reported size: %" PRId32 "\n",contentsize);

	// Start with a single page buffer
	buf = (u8*)malloc(0x1000);
	if(buf==NULL){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return -1;
	}

	do {
		// This download loop resizes the buffer as data is read.
		ret = httpcDownloadData(&context, buf+size, 0x1000, &readsize);
		size += readsize; 
		if (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING){
				lastbuf = buf; // Save the old pointer, in case realloc() fails.
				buf = (u8*)realloc(buf, size + 0x1000);
				if(buf==NULL){ 
					httpcCloseContext(&context);
					free(lastbuf);
					if(newurl!=NULL) free(newurl);
					return -1;
				}
			}
	} while (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING);	

	if(ret!=0){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		free(buf);
		return -1;
	}

	// Resize the buffer back down to our actual final size
	lastbuf = buf;
	buf = (u8*)realloc(buf, size);
	if(buf==NULL){ // realloc() failed.
		httpcCloseContext(&context);
		free(lastbuf);
		if(newurl!=NULL) free(newurl);
		return -1;
	}

	//printf("downloaded size: %" PRId32 "\n",size);

	//if(size>(240*400*3*2))size = 240*400*3*2;
	
	*outputBuf = buf;

	//framebuf_top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	//memcpy(framebuf_top, buf, size);
    //
	//gfxFlushBuffers();
	//gfxSwapBuffers();
    //
	//framebuf_top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	//memcpy(framebuf_top, buf, size);
    //
	//gfxFlushBuffers();
	//gfxSwapBuffers();
	//gspWaitForVBlank();
    //
	//httpcCloseContext(&context);
	//free(buf);
	if (newurl!=NULL) free(newurl);

	return size;
}

// responsible for freeing buf
u32 http_post(const char* url, const char* data, char** charBuf)
{
	Result ret=0;
	httpcContext context;
	char *newurl=NULL;
	u32 statuscode=0;
	u32 contentsize=0, readsize=0, size=0;
	u8 *buf, *lastbuf;

	prepStatus();
	printf("POSTing %s%s\n", url, data);

	do {
		ret = httpcOpenContext(&context, HTTPC_METHOD_POST, url, 0);
		//printf("return from httpcOpenContext: %" PRIx32 "\n",ret);

		// This disables SSL cert verification, so https:// will be usable
		ret = httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);
		//printf("return from httpcSetSSLOpt: %" PRIx32 "\n",ret);

		// Enable Keep-Alive connections
		ret = httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED);
		//printf("return from httpcSetKeepAlive: %" PRIx32 "\n",ret);

		// Set a User-Agent header so websites can identify your application
		ret = httpcAddRequestHeaderField(&context, "User-Agent", "httpc-example/1.0.0");
		//printf("return from httpcAddRequestHeaderField: %" PRIx32 "\n",ret);

		// Set a Content-Type header so websites can identify the format of our raw body data.
		// If you want to send form data in your request, use:
		ret = httpcAddRequestHeaderField(&context, "Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
		// If you want to send raw JSON data in your request, use:
		//ret = httpcAddRequestHeaderField(&context, "Content-Type", "application/json");
		//printf("return from httpcAddRequestHeaderField: %" PRIx32 "\n",ret);

		// Post specified data.
		// If you want to add a form field to your request, use:
		//ret = httpcAddPostDataAscii(&context, "data", value);
		// If you want to add a form field containing binary data to your request, use:
		//ret = httpcAddPostDataBinary(&context, "field name", yourBinaryData, length);
		// If you want to add raw data to your request, use:
		ret = httpcAddPostDataRaw(&context, (u32*)data, strlen(data));
		//printf("return from httpcAddPostDataRaw: %" PRIx32 "\n",ret);

		ret = httpcBeginRequest(&context);
		if(ret!=0){
			httpcCloseContext(&context);
			if(newurl!=NULL) free(newurl);
			return ret;
		}

		ret = httpcGetResponseStatusCode(&context, &statuscode);
		if(ret!=0){
			httpcCloseContext(&context);
			if(newurl!=NULL) free(newurl);
			return ret;
		}

		if ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308)) {
			if(newurl==NULL) newurl = malloc(0x1000); // One 4K page for new URL
			if (newurl==NULL){
				httpcCloseContext(&context);
				return -1;
			}
			ret = httpcGetResponseHeader(&context, "Location", newurl, 0x1000);
			url = newurl; // Change pointer to the url that we just learned
			//printf("redirecting to url: %s\n",url);
			httpcCloseContext(&context); // Close this context before we try the next
		}
	} while ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308));

	if(statuscode!=200){
		//printf("URL returned status: %" PRIx32 "\n", statuscode);
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return -2;
	}

	// This relies on an optional Content-Length header and may be 0
	ret=httpcGetDownloadSizeState(&context, NULL, &contentsize);
	if(ret!=0){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return ret;
	}

	//printf("reported size: %" PRIx32 "\n",contentsize);

	// Start with a single page buffer
	buf = (u8*)malloc(0x1000);
	if(buf==NULL){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return -1;
	}

	do {
		// This download loop resizes the buffer as data is read.
		ret = httpcDownloadData(&context, buf+size, 0x1000, &readsize);
		size += readsize; 
		if (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING){
				lastbuf = buf; // Save the old pointer, in case realloc() fails.
				buf = realloc(buf, size + 0x1000);
				if(buf==NULL){ 
					httpcCloseContext(&context);
					free(lastbuf);
					if(newurl!=NULL) free(newurl);
					return -1;
				}
			}
	} while (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING);	

	if(ret!=0){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		free(buf);
		return -1;
	}

	// Resize the buffer back down to our actual final size
	lastbuf = buf;
	buf = realloc(buf, size);
	if(buf==NULL){ // realloc() failed.
		httpcCloseContext(&context);
		free(lastbuf);
		if(newurl!=NULL) free(newurl);
		return -1;
	}

	*charBuf = (char*)buf;
	//printf("response size: %" PRIx32 "\n",size);

	// Print result
	//printf((char*)buf);
	//printf("\n");
	
	//gfxFlushBuffers();
	//gfxSwapBuffers();
    
	httpcCloseContext(&context);
	//free(buf);
	if (newurl!=NULL) free(newurl);

	return size;
}


void clearOptions()
{
	for (int i = 3; i < 17; i += 2)
	{
		for (int j = 0; j < 50; ++j)
		{
			printf("\x1b[%d;%dH ", i, j);
		}
	}
}

void printOption(const char* text, int optionIdx)
{
	bool selected = optionIdx == g_curOptionIdx;
	if (selected)
	{
		// Change the bgcolor to blue
		printf("\x1b[44m");
	}
	
	printf(text);
	
	if (selected)
	{
		// Reset
		printf("\x1b[0m");
	}
}

void drawOptions()
{
	clearOptions();
	
	printf("\x1b[1;8HGenerate Item Randomizer Game (v1.0)");
	char tmpStr[80];
	int optionIdx = 0;
	
	sprintf(tmpStr, "\x1b[3;2HMode: %s", g_modes[g_curState.m_curMode]);
	printOption(tmpStr, optionIdx++);
	
	sprintf(tmpStr, "\x1b[5;2HGoal: %s", g_goals[g_curState.m_curGoal]);
	printOption(tmpStr, optionIdx++);
	
	sprintf(tmpStr, "\x1b[7;2HSeed: random");
	printOption(tmpStr, optionIdx++);
	
	sprintf(tmpStr, "\x1b[9;2HLogic: %s", g_logics[g_curState.m_curLogic]);
	printOption(tmpStr, optionIdx++);
	
	sprintf(tmpStr, "\x1b[11;2HDifficulty: %s", g_dificulties[g_curState.m_curDifficulty]);
	printOption(tmpStr, optionIdx++);
	
	sprintf(tmpStr, "\x1b[13;2HVariation: %s", g_variations[g_curState.m_curVariation]);
	printOption(tmpStr, optionIdx++);
	
	sprintf(tmpStr, "\x1b[15;2HSprite: %s", g_sprites[g_curState.m_curSprite]);
	printOption(tmpStr, optionIdx++);
}

void initState()
{
	g_curState.m_curMode = 0;
	g_curState.m_curGoal = 0;
	g_curState.m_curLogic = 0;
	g_curState.m_curDifficulty = 1; // Normal
	g_curState.m_curVariation = 0;
	g_curState.m_curSprite = 0;
}

void handleSwitchChoice(int* toSwitch, bool increase, int max)
{
	if (increase)
	{
		(*toSwitch)++;
		if (*toSwitch >= max)
		{
			*toSwitch = 0;
		}
	}
	else
	{
		// decrease
		(*toSwitch)--;
		if (*toSwitch < 0)
		{
			*toSwitch = max - 1;
		}
	}
}

void switchChoice(bool increase)
{
	switch (g_curOptionIdx)
	{
		case 0:
			handleSwitchChoice(&g_curState.m_curMode, increase, NUM_MODES);
			break;
		case 1:
			handleSwitchChoice(&g_curState.m_curGoal, increase, NUM_GOALS);
			break;
		case 3:
			handleSwitchChoice(&g_curState.m_curLogic, increase, NUM_LOGICS);
			break;
		case 4:
			handleSwitchChoice(&g_curState.m_curDifficulty, increase, NUM_DIFICULTIES);
			break;
		case 5:
			handleSwitchChoice(&g_curState.m_curVariation, increase, NUM_VARIATIONS);
			break;
		case 6:
			handleSwitchChoice(&g_curState.m_curSprite, increase, NUM_SPRITES);
			break;
	}
}

void initModes()
{
	g_modes[0] = "standard";
	g_modes[1] = "open";
	g_modes[2] = "swordless";
}

void initGoals()
{
	g_goals[0] = "ganon";
	g_goals[1] = "dungeons";
	g_goals[2] = "pedestal";
	g_goals[3] = "triforce-hunt";
}

void initLogics()
{
	g_logics[0] = "NoMajorGlitches";
	g_logics[1] = "OverworldGlitches";
	g_logics[2] = "MajorGlitches";
}

void initDificulties()
{
	g_dificulties[0] = "easy";
	g_dificulties[1] = "normal";
	g_dificulties[2] = "hard";
	g_dificulties[3] = "expert";
	g_dificulties[4] = "insane";
}

void initVariations()
{
	g_variations[0] = "none";
	g_variations[1] = "timed-race";
	g_variations[2] = "timed-ohko";
	g_variations[3] = "ohko";
	g_variations[4] = "triforce-hunt";
	g_variations[5] = "key-sanity";
}

void initSprites()
{
	int i = 0;
	g_sprites[i++] = "link.1.spr";
	g_sprites[i++] = "4slink-armors.1.spr";
	g_sprites[i++] = "boo.2.spr";
	g_sprites[i++] = "boy.2.spr";
	g_sprites[i++] = "cactuar.1.spr";
	g_sprites[i++] = "cat.1.spr";
	g_sprites[i++] = "catboo.1.spr";
	g_sprites[i++] = "cirno.1.spr";
	g_sprites[i++] = "darkboy.2.spr";
	g_sprites[i++] = "darkgirl.1.spr";
	g_sprites[i++] = "darklink.1.spr";
	g_sprites[i++] = "shadowsaku.1.spr";
	g_sprites[i++] = "darkswatchy.1.spr";
	g_sprites[i++] = "darkzelda.1.spr";
	g_sprites[i++] = "darkzora.2.spr";
	g_sprites[i++] = "decidueye.1.spr";
	g_sprites[i++] = "demonlink.1.spr";
	g_sprites[i++] = "froglink.2.spr";
	g_sprites[i++] = "ganondorf.1.spr";
	g_sprites[i++] = "garfield.1.spr";
	g_sprites[i++] = "girl.2.spr";
	g_sprites[i++] = "headlesslink.1.spr";
	g_sprites[i++] = "invisibleman.1.spr";
	g_sprites[i++] = "inkling.1.spr";
	g_sprites[i++] = "kirby-meta.2.spr";
	g_sprites[i++] = "kore8.1.spr";
	g_sprites[i++] = "littlepony.1.spr";
	g_sprites[i++] = "luigi.1.spr";
	g_sprites[i++] = "maiden.2.spr";
	g_sprites[i++] = "maplequeen.1.spr";
	g_sprites[i++] = "mario-classic.1.spr";
	g_sprites[i++] = "marisa.1.spr";
	g_sprites[i++] = "mikejones.2.spr";
	g_sprites[i++] = "minishcaplink.3.spr";
	g_sprites[i++] = "modernlink.1.spr";
	g_sprites[i++] = "mog.1.spr";
	g_sprites[i++] = "mouse.1.spr";
	g_sprites[i++] = "naturelink.1.spr";
	g_sprites[i++] = "negativelink.1.spr";
	g_sprites[i++] = "neonlink.1.spr";
	g_sprites[i++] = "oldman.1.spr";
	g_sprites[i++] = "pinkribbonlink.1.spr";
	g_sprites[i++] = "popoi.1.spr";
	g_sprites[i++] = "pug.2.spr";
	g_sprites[i++] = "purplechest-bottle.2.spr";
	g_sprites[i++] = "roykoopa.1.spr";
	g_sprites[i++] = "rumia.1.spr";
	g_sprites[i++] = "samus.4.spr";
	g_sprites[i++] = "sodacan.1.spr";
	g_sprites[i++] = "staticlink.1.spr";
	g_sprites[i++] = "santalink.1.spr";
	g_sprites[i++] = "superbunny.1.spr";
	g_sprites[i++] = "swatchy.1.spr";
	g_sprites[i++] = "tingle.1.spr";
	g_sprites[i++] = "toad.1.spr";
	g_sprites[i++] = "valeera.1.spr";
	g_sprites[i++] = "vitreous.1.spr";
	g_sprites[i++] = "vivi.1.spr";
	g_sprites[i++] = "will.1.spr";
	g_sprites[i++] = "wizzrobe.4.spr";
	g_sprites[i++] = "yunica.1.spr";
	g_sprites[i++] = "zelda.2.spr";
	g_sprites[i++] = "zerosuitsamus.1.spr";
	g_sprites[i++] = "zora.1.spr";
}

bool handleROMLoad()
{
	FILE* fExpanded = fopen("alttp_expanded.sfc", "rb");
	if (fExpanded == NULL)
	{
		FILE* fOrig = fopen("alttp.sfc", "rb");
		if (fOrig == NULL)
		{
			// No original file =(
			prepStatus();
			printf("Please add an original rom named alttp.sfc to the program's folder\n");
			fclose(fOrig);
			fclose(fExpanded);
			return false;
		}
		
		// TODO: Check expanded file, check checksum of original
		prepStatus();
		printf("Downloading one time initial patch...\n");
		char* buf;
		http_post("http://vt.alttp.run/build/js/base2current-5d0b568068.json", 
			"", 
			&buf);
			
		prepStatus();
		printf("Performing one time initial patch...\n");
			
		parseJSON(buf, "alttp.sfc", "alttp_expanded.sfc");
		free(buf);
	}
	fclose(fExpanded);
	return true;
}

// Get the query string to request the randomized rom
char* getQueryString()
{
	memset(&queryString[0], 0, sizeof(queryString));
	sprintf(&queryString[0],
		"logic=%s&difficulty=%s&variation=%s&mode=%s&goal=%s&heart_speed=half&sram_trace=false&menu_fast=true&debug=false&tournament=false",
		g_logics[g_curState.m_curLogic],
		g_dificulties[g_curState.m_curDifficulty],
		g_variations[g_curState.m_curVariation],
		g_modes[g_curState.m_curMode],
		g_goals[g_curState.m_curGoal]);
	return &queryString[0];
}

// Patch the selected SPR file onto the randomized rom
void patchSprite()
{
	const char* curSprite = g_sprites[g_curState.m_curSprite];
	char* url = (char*)calloc(90 + sizeof(curSprite), sizeof(char));
	strcpy(url, "http://a4482918739889ddcb78-781cc7889ba8761758717cf14b1800b4.r32.cf2.rackcdn.com/");
	strcat(url, curSprite);
	prepStatus();
	printf("Downloading sprite\n");

	// Get spr file
	u8* spr;
	u32 sprSize = http_download(url, &spr);
	prepStatus();
	free(url);
	
	if (sprSize < 1)
	{
		prepStatus();
		printf("Failed to download spr, not patching sprite\n");
		return;
	}
	
	FILE* file = fopen(seedFileName, "rb");
	if (file == NULL)
	{
		prepStatus();
		printf("%s could not be loaded\n", seedFileName);
		return;
	}
	prepStatus();
	printf("Reading file for sprite patch %s\n", seedFileName);
	u8* rom = (u8*)calloc(TWO_MB, sizeof(u8));
	fread(rom, TWO_MB, 1, file);
	fclose(file);
	
	for (int i = 0; i < 0x7000; ++i)
	{
		rom[0x80000 + i] = spr[i];
	}
	for (int i = 0; i < 120; ++i)
	{
		rom[0xDD308 + i] = spr[0x7000 + i];
	}
	// gloves color
	rom[0xDEDF5] = spr[0x7036];
	rom[0xDEDF6] = spr[0x7037];
	rom[0xDEDF7] = spr[0x7054];
	rom[0xDEDF8] = spr[0x7055];
	
	FILE* writeFile;
	writeFile = fopen(seedFileName, "wb");
	fwrite(rom, TWO_MB, 1, writeFile);	
	fclose(writeFile);
	free(rom);
	free(spr);
	
	prepStatus();
	printf("Patched sprite %s to file %s\n", g_sprites[g_curState.m_curSprite], seedFileName);
}

int main(int argc, char **argv)
{
	gfxInitDefault();
	httpcInit(4 * 1024 * 1024); // Buffer size when POST/PUT.
	
	g_curOptionIdx = 0;
	initState();
	initModes();
	initGoals();
	initLogics();
	initDificulties();
	initVariations();
	initSprites();

	//Initialize console on top screen. Using NULL as the second argument tells the console library to use the internal console structure as current one
	consoleInit(GFX_TOP, NULL);

	handleROMLoad();
	drawOptions();

	prepStatus();
	printf("Press A to generate ROM!");

	//We don't need double buffering in this example. In this way we can draw our image only once on screen.
	gfxSetDoubleBuffering(GFX_BOTTOM, false);

	//Get the bottom screen's frame buffer
	u8* fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
	
	//Copy our image in the bottom screen's frame buffer
	memcpy(fb, brew_bgr, brew_bgr_size);

	// Main loop
	while (aptMainLoop())
	{
		//Scan all the inputs. This should be done once for each frame
		hidScanInput();

		//hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
		u32 kDown = hidKeysDown();

		if (kDown & KEY_START) break; // break in order to return to hbmenu
		
		if (kDown & KEY_A)
		{
			char* buf;
			http_post("http://vt.alttp.run/seed", 
				//"logic=NoMajorGlitches&difficulty=normal&variation=none&mode=open&goal=ganon&heart_speed=half&sram_trace=false&menu_fast=true&debug=false&tournament=false", 
				getQueryString(),
				&buf);
				
			parseJSON(buf, "alttp_expanded.sfc", "alttp_seed.sfc");
			free(buf);
			
			patchSprite();
		}
		
		// Toggle options
		if (kDown & KEY_DDOWN)
		{
			g_curOptionIdx = (g_curOptionIdx + 1) % NUM_OPTIONS;
			drawOptions();
		}
		if (kDown & KEY_DUP)
		{
			--g_curOptionIdx;
			if (g_curOptionIdx == -1)
			{
				g_curOptionIdx = NUM_OPTIONS - 1;
			}
			drawOptions();
		}
		
		// Switch current selection
		if (kDown & KEY_DRIGHT)
		{
			switchChoice(true);
			drawOptions();
		}
		if (kDown & KEY_DLEFT)
		{
			switchChoice(false);
			drawOptions();
		}

		// Flush and swap framebuffers
		gfxFlushBuffers();
		gfxSwapBuffers();

		//Wait for VBlank
		gspWaitForVBlank();
	}

	// Exit services
	gfxExit();
	return 0;
}
