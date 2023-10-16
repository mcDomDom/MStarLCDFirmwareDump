// MStarLCDFirmwareDump.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"

#define	_SEP_	","

#pragma pack(push, 1) 
	struct T_WH
	{
		WORD	width;
		WORD	height;
	};
	struct T_Preset_14	// 14byte BE MX279H
	{
		BYTE	pol;	// polarity flag 0x1f:interlace?
		BYTE	idx;	// width/height table index
		WORD	hFreq;	// horizontal freq x10
		WORD	vFreq;	// vertical freq x10
		WORD	hTotal;	// horizontal total (back porch + width + sync + front proch)
		WORD	vTotal;	// vertical total (back porch + height + sync + front proch)
		WORD	hSBP;	// horizontal sync+back porch
		WORD	vSBP;	// verrical sync+back porch
	};
	struct T_Preset_15	// 15byte BE EA224WMi,P1917S等
	{
		BYTE	pol;	// polarity flag 0x1f:interlace?
		BYTE	idx;	// width/height table index
		WORD	hFreq;	// horizontal freq x10
		WORD	vFreq;	// vertical freq x10
		WORD	hTotal;	// horizontal total (back porch + width + sync + front proch)
		WORD	vTotal;	// vertical total (back porch + height + sync + front proch)
		WORD	hSBP;	// horizontal sync+back porch
		WORD	vSBP;	// verrical sync+back porch
		BYTE	nazo;
	};
	struct T_Preset_20	// 20byte LE EA245WMiで確認 要decomp
	{
		DWORD	pol;	// polarity flag? 0x1?でインタレース
		DWORD	idx;	// width/height table index
		WORD	hFreq;	// horizontal freq x10
		WORD	vFreq;	// vertical freq x10
		WORD	hTotal;	// horizontal total (back porch + width + sync + front proch)
		WORD	vTotal;	// vertical total (back porch + height + sync + front proch)
		WORD	hSBP;	// horizontal sync+back porch
		WORD	vSBP;	// verrical sync+back porch
	};
	struct T_Preset_20_2	// 20byte LE BNC59Hで確認 要decomp
	{
		DWORD	idx;	// width/height table index
		WORD	hFreq;	// horizontal freq x10
		WORD	vFreq;	// vertical freq x10
		WORD	hTotal;	// horizontal total (back porch + width + sync + front proch)
		WORD	vTotal;	// vertical total (back porch + height + sync + front proch)
		WORD	hSBP;	// horizontal sync+back porch
		WORD	vSBP;	// verrical sync+back porch
		DWORD	pol;	// polarity flag? 0x1?でインタレース
	};
	struct T_Preset_24	// 24byte LE T.V56.031/1280-IPSで確認 要decomp
	{
		DWORD	idx;	// width/height table index
		WORD	hFreq;	// horizontal freq x10
		WORD	vFreq;	// vertical freq x10
		WORD	hSBP;	// horizontal sync+back porch
		WORD	vSBP;	// verrical sync+back porch
		WORD	hTotal;	// horizontal total (back porch + width + sync + front proch)
		WORD	vTotal;	// vertical total (back porch + height + sync + front proch)
		WORD	nazo[2];// hporch,vporch?
		DWORD	pol;	// polarity flag? 0x1?でインタレース
	};

	
#pragma pack(pop) 

enum enMode {
	UNKNOWN = 0,
	EA224WMi,
	EA245WMi,
	P1914S,
	P1917S,
	P2421H,
	MX279H,
	T2454p,	
	TV56031,	// T.V056.31 LCD Controller Board
	BNC59H,		// amiph 15.6 FullHD ips
	KP1208IPS	// KAPPY.氏1280-IPS

};

enum enReturnCode {
	RC_SUCCESS,
	RC_OPEN_ERROR,
	RC_COMP_FORM,
	RC_UNKNOWN_FIRM,
	RC_WRITE_ERROR,

};


BYTE buf[1024*1024*4];
int file_len;
int wh_ofs;
int wh_num;
int tbl_ofs;
int tbl_num;


/**
		圧縮バイナリかチェック
*/
bool CheckCompress()
{
	bool	bRet = false;
	BYTE	magic[3] = {0xBE, 0xAD, 0xDE};

	for (int i=0x20000; i<file_len-3; i++) {
		if (memcmp(&buf[i], magic, 3) == 0) {
			bRet = true;
			break;
		}
	}

	return bRet;
}

/**
		ファイル名からモードを判定
*/
enMode JudgeMode(const char *szFname)
{
	char	szFnameUpper[_MAX_FNAME]; 

	enMode mode = UNKNOWN;
	memset(szFnameUpper, 0x00, sizeof(szFnameUpper));
	for (int i=0; i<(int)strlen(szFname); i++) szFnameUpper[i] = toupper(szFname[i]);
	if (strstr(szFnameUpper, "EA224WM"))		mode = EA224WMi;
	else if (strstr(szFnameUpper, "EA245WM"))	mode = EA245WMi;
	else if (strstr(szFnameUpper, "P1914"))		mode = P1914S;
	else if (strstr(szFnameUpper, "P1917"))		mode = P1917S;
	else if (strstr(szFnameUpper, "P2421"))		mode = P2421H;
	else if (strstr(szFnameUpper, "MX279"))		mode = MX279H;
	else if (strstr(szFnameUpper, "T2454"))		mode = T2454p;
	else if (strstr(szFnameUpper, "TV56031") || strstr(szFnameUpper, "T.V56.031"))		mode = TV56031;
	else if (strstr(szFnameUpper, "AMIPH") || strstr(szFnameUpper, "BNC59"))			mode = BNC59H;
	else if (strstr(szFnameUpper, "1208-IPS"))	mode = KP1208IPS;

	return mode;
}

/**
		幅・高さテーブルを検索
*/
int FindWidthHeightTable(
int		&nCount,	//!< o	:定義数
bool	bBE			//!< i	:Big Endianか否か
)
{
	int nRet = -1;
	for (int pos=0; pos<file_len-65536; pos++) {	// 多分先頭256k以内にあるやろ？
		int nOldHit = -1;
		nCount = 0;
		for (int i=0; i<256; i++) {
			T_WH *pWh = (T_WH *)&buf[pos+sizeof(T_WH)*i];
			int width = bBE ? ntohs(pWh->width) : pWh->width;
			int height =  bBE ? ntohs(pWh->height) : pWh->height;
			if (640 <= width && width <= 2560 && width%2==0 &&
				200 <= height && height <= 2048 && height%2==0 &&
				(nOldHit == -1 || i == nOldHit+1)) {
				nCount++;
				nOldHit = i;
			}
			else {
				break;
			}
		}
		if (20 < nCount) {
			nRet = pos;
			break;
		}
	}

	return nRet;
}

/**
		プリセットテーブルを検索
*/
int FindPresetTable(
int		&nCount,	//!< o	:定義数
bool	bBE			//!< i	:Big Endianか否か
)
{
	int nRet = -1, nStart = -1;
	for (int pos=0; pos<1024*256; pos++) {	// 多分先頭256k以内にあるやろ？
		int nOldHit = -1;
		nCount = 0;
		for (int i=0; i<256; i++) {
			T_Preset_15 *pInfo = (T_Preset_15 *)&buf[pos+sizeof(T_Preset_15)*i];
			int hfreq = bBE ? ntohs(pInfo->hFreq) : pInfo->hFreq;
			int vfreq = bBE ? ntohs(pInfo->vFreq) : pInfo->vFreq;
			int htotal = bBE ? ntohs(pInfo->hTotal) : pInfo->hTotal;
			int vtotal = bBE ? ntohs(pInfo->vTotal) : pInfo->vTotal;
			if (140 <= hfreq && hfreq <= 1400 &&
				200 <= vfreq && vfreq <= 1200 &&
				700 <= htotal && htotal <= 4200 &&
				250 <= vtotal && vtotal <= 2300 &&
				(nOldHit == -1 || nOldHit+1 == i)) {
				if (nCount == 0) nStart = pos;
				nCount++;
				nOldHit = i;
			}
			else {
				break;
			}
		}
		if (50 <= nCount) {
			nRet = nStart;
			break;
		}
	}

	return nRet;
}

/**
		Big Endianか否か
*/
bool IsBigEndian(enMode mode)
{
	bool bRet = true;

	if (mode == EA245WMi || 
		mode == TV56031 || 
		mode == BNC59H || 
		mode == KP1208IPS) {
		bRet = false;
	}

	return bRet;
}

template <typename T>
void SetAddParameter(T *pInfo, BYTE nazo)
{
}
template <>
void SetAddParameter(T_Preset_15 *pInfo, BYTE nazo)
{
	if (0 < nazo) pInfo->nazo = nazo;
}


/**
		プリセットテーブルのパラメータを設定する
*/
template <typename T>
void SetParameter(bool bBE, int tbl_no, BYTE pol, BYTE idx, WORD hFreq, WORD vFreq, WORD hTotal, WORD vTotal, WORD hSBP, WORD vSBP, WORD width, WORD height, BYTE nazo=0x00)
{
	T	*pInfo = (T *)&buf[tbl_ofs+sizeof(T)*tbl_no];
	if (0 <= pol) pInfo->pol = pol;
	if (0 <= idx) pInfo->idx = idx;
	if (0 < hFreq) pInfo->hFreq = bBE ? ntohs(hFreq) : hFreq;
	if (0 < vFreq) pInfo->vFreq = bBE ? ntohs(vFreq) : vFreq;
	if (0 < hTotal) pInfo->hTotal = bBE ? ntohs(hTotal) : hTotal;
	if (0 < vTotal) pInfo->vTotal = bBE ? ntohs(vTotal) : vTotal;
	if (0 < hSBP) pInfo->hSBP = bBE ? ntohs(hSBP) : hSBP;
	if (0 < vSBP) pInfo->vSBP = bBE ? ntohs(vSBP) : vSBP;
	SetAddParameter(pInfo, nazo);
	if (0 <= idx && 0 < width && 0 < height) {
		T_WH	*pWH = (T_WH *)&buf[wh_ofs+sizeof(T_WH)*idx];
		pWH->width = bBE ? ntohs(width) : width;
		pWH->height = bBE ? ntohs(height): height;
	}
}


template <typename T>
char *GetAddStr(T *pInfo, bool bBE)
{
	static char szAddStr[32];
	return szAddStr;
}
template <>
char *GetAddStr(T_Preset_15 *pInfo, bool bBE)
{
	static char szAddStr[32];
	sprintf(szAddStr, _SEP_"0x%02X(%d)", pInfo->nazo, pInfo->nazo);
	return szAddStr;
}
template <>
char *GetAddStr(T_Preset_24 *pInfo, bool bBE)
{
	static char szAddStr[32];
	sprintf(szAddStr, _SEP_"0x%02X(%d)"_SEP_"0x%02X(%d)", pInfo->nazo[0], pInfo->nazo[0], pInfo->nazo[1], pInfo->nazo[1]);
	return szAddStr;
}

/**
		プリセットテーブルのパラメータを表示する
*/
template <typename T>
void DumpPreset(int i, bool bBE)
{
	int pos = tbl_ofs+i*sizeof(T);
	T *pInfo = (T *)&buf[pos];
	int pol = pInfo->pol;
	int idx = pInfo->idx;
	int hFreq = bBE ? htons(pInfo->hFreq) : pInfo->hFreq;
	int vFreq = bBE ? htons(pInfo->vFreq) : pInfo->vFreq;
	int hTotal = bBE ? htons(pInfo->hTotal) : pInfo->hTotal;
	int vTotal = bBE ? htons(pInfo->vTotal) : pInfo->vTotal;
	int hSBP = bBE ? htons(pInfo->hSBP) : pInfo->hSBP;
	int vSBP = bBE ? htons(pInfo->vSBP) : pInfo->vSBP;

	int whpos = wh_ofs+idx*sizeof(T_WH);
	T_WH *pWh = (T_WH *)&buf[whpos];
	int width = bBE ? htons(pWh->width) : pWh->width;
	int height = bBE ? htons(pWh->height) : pWh->height;

	printf(
		"0x%06x"_SEP_	// pos
		"%3d"_SEP_		// i
		"0x%02x"_SEP_	// pol
		"%3d"_SEP_		// idx
		"%4d"_SEP_		// width
		"%4d"_SEP_		// height
		"%4d"_SEP_		// hFreq
		"%4d"_SEP_		// vFreq
		"%4d"_SEP_		// hTotal
		"%4d"_SEP_		// vTotal
		"%4d"_SEP_		// hSBP
		"%4d"			// vSBP
		"%s\n", 
		pos, i, pol, idx, width, height, hFreq, vFreq, hTotal, vTotal, hSBP, vSBP, GetAddStr(pInfo, bBE));
}
int _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 2) {
		printf("%s firmware.bin (-modify)\n", argv[0]);
		return 0;
	}

	FILE *fp = fopen(argv[1], "rb");
	if (!fp) {
		printf("can't open %s\n", argv[1]);
		return RC_OPEN_ERROR;
	}
	file_len = _filelength(_fileno(fp));
	fread(buf, file_len, 1, fp);
	fclose(fp);

	if (CheckCompress()) {
		printf("compressed firmware\n");
		return RC_COMP_FORM;
	}

	char	szPath[_MAX_PATH], szDrive[_MAX_DRIVE], szDir[_MAX_DIR], szFname[_MAX_FNAME], szExt[_MAX_EXT]; 
	_splitpath(argv[1], szDrive, szDir, szFname, szExt);
	enMode mode = JudgeMode(szFname);
	if (mode == EA224WMi) {
		wh_ofs = 0x14000;	
		wh_num = 38;		
		tbl_ofs = 0x14098;	
		tbl_num = 116;		
	}
	else if (mode == EA245WMi) {
		wh_ofs = 0xBB160+0x4B98;	
		wh_num = 47;		
		tbl_ofs = 0xBB21C+0x4B98;	
		tbl_num = 128;		
	}
	else if (mode == P1914S) {
		wh_ofs = 0x014A3A;
		wh_num = 46;
		tbl_ofs = 0x14AEA;
		tbl_num = 123;
	}
	else if (mode == P1917S) {
		wh_ofs = 0x014A35;
		wh_num = 45;
		tbl_ofs = 0x14AE9;
		tbl_num = 124;
	}
	else if (mode == P2421H) {
		wh_ofs = 0x014A56;
		wh_num = 47;
		tbl_ofs = 0x14b12;
		tbl_num = 129;
	}
	else if (mode == MX279H) {
		wh_ofs = 0x044500;
		wh_num = 28;
		tbl_ofs = 0x044570;
		tbl_num = 69;
	}
	else if (mode == T2454p) {
		wh_ofs = 0x2C00;
		wh_num = 50;
		tbl_ofs = 0x2CC8;
		tbl_num = 128;
	}
	else if (mode == TV56031) {
		wh_ofs = 0x17cbc0;
		wh_num = 35;
		tbl_ofs = 0x1bf428;
		tbl_num = 87;
	}
	else if (mode == BNC59H) {	// 20byte
		wh_ofs = 0x1291f4;
		wh_num = 35;
		tbl_ofs = 0x16c05c;
		tbl_num = 187;
	}
	else if (mode == KP1208IPS) {
		wh_ofs = 0x172728;
		wh_num = 35;
		tbl_ofs = 0x1b5548;
		tbl_num = 86;
	}
	else {
		// 機種指定が無い場合は機種の多そうな15byteプリセットが適用できるか検索
		int nWidthHeightCount, nPresetCount;
		int nWidthHeightOffset = FindWidthHeightTable(nWidthHeightCount, true);
		int nPresetOffset = FindPresetTable(nPresetCount, true);
		/* 
		// 念のためLittle Endianでも探してみる
		if (nWidthHeightOffset < 0 || nPresetOffset < 0) {
			nWidthHeightOffset = FindWidthHeightTable(nWidthHeightCount, false);
			nPresetOffset = FindPresetTable(nPresetCount, false);
		}
		*/
		if (nWidthHeightOffset < 0 || nPresetOffset < 0) {
			printf("%s unknown firmware\n", argv[1]);
			return RC_UNKNOWN_FIRM;
		}
		wh_ofs = nWidthHeightOffset;
		wh_num = nWidthHeightCount;
		tbl_ofs = nPresetOffset;
		tbl_num = nPresetCount;
		mode = EA224WMi;
	}

	if (2 < argc && _stricmp(argv[2], "-modify") == 0 && 0 <= wh_ofs && 0 <= tbl_ofs ) {
		if (mode == EA224WMi) {
			SetParameter<T_Preset_15>(true,   2, 0x0f,  4, 315, 555,  1104, 567, 263, 41,  768, 512);		// X68000  768x512 31KHz
			SetParameter<T_Preset_15>(true,  20, 0x0f,  5, 159, 614,  608, 260,  40, 17,  512, 240, 0x2);	// X68000  512x512 15KHz(interlace)
			SetParameter<T_Preset_15>(true, 115, 0x0f,  6, 247, 531, 1408, 465, 288, 33, 1024, 424);		// X68000 1024x424 24KHz
			SetParameter<T_Preset_15>(true,  18, 0x0f,  8, 245, 524,  944, 469, 240, 21,  640, 448);		// X68000 Fantasy Zone 24KHz
			SetParameter<T_Preset_15>(true,  21, 0x0f,  9, 315, 543, 1178, 580, 308, 38,  768, 536);		// X68000 Dash Yarou 31KHz
			SetParameter<T_Preset_15>(true,  14, 0x0f, 11, 315, 529, 1104, 595, 360, 32,  672, 560);		// X68000 Druaga 31KHz  
			SetParameter<T_Preset_15>(true,  26, 0x1f, 12, 247, 531, 1408, 930, 288, 33, 1020, 848, 0x0);	// X68000 1024x848(interlace) 24KHz 偶数・奇数ライン逆？
			SetParameter<T_Preset_15>(true,  45, 0x1f, 15, 159, 617,  608, 525,  80, 25,  516, 480);		// X68000  512x512 15KHz
			// tbl:101 idx:32 480i
			// tbl:  9 idx:33 480p
			// tbl:102 idx:35 240p(60Hz)
			// tbl:103 idx:35 240p(50Hz)
			//         idx:34は未使用?
		}
		else if (mode == EA245WMi) {
			SetParameter<T_Preset_20>(false,  9, 0x00,  4, 315, 555, 1104, 567, 263, 41,  768, 512);		// X68000  768x512 31KHz
			SetParameter<T_Preset_20>(false, 22, 0x11, 12, 159, 614,  608, 260,  40, 17,  512, 240);		// X68000  512x512 15KHz(interlace)
			SetParameter<T_Preset_20>(false, 23, 0x01, 11, 247, 531, 1408, 465, 288, 33, 1024, 424);		// X68000 1024x424 24KHz
			SetParameter<T_Preset_20>(false, 30, 0x01, 14, 245, 524,  944, 469, 240, 21,  640, 448);		// X68000 Fantasy Zone 24KHz
			SetParameter<T_Preset_20>(false, 56, 0x01, 17, 315, 543, 1178, 580, 308, 38,  768, 536);		// X68000 Dash Yarou 31KHz
			SetParameter<T_Preset_20>(false, 60, 0x01, 18, 315, 529, 1104, 595, 360, 32,  672, 560);		// X68000 Druaga 31KHz  
			SetParameter<T_Preset_20>(false, 72, 0x11, 23, 247, 531, 1408, 930, 288, 33, 1020, 848);		// X68000 1024x848(interlace) 24KHz 偶数・奇数ライン逆？
			SetParameter<T_Preset_20>(false,113, 0x01, 25, 159, 617,  608, 525,  80, 25,  516, 480);		// X68000  512x512 15KHz
			// tbl:101 idx:32 480i
			// tbl:  9 idx:33 480p
			// tbl:102 idx:35 240p(60Hz)
			// tbl:103 idx:35 240p(50Hz)
			//         idx:34は未使用?
		}
		else if (mode == P1914S || mode == P1917S) {
			// P1914S/P1917S
			SetParameter<T_Preset_15>(true,   2, 0x0f,  4, 315, 555, 1104, 567, 263, 41,  768, 512);		// X68000  768x512 31KHz
			SetParameter<T_Preset_15>(true,  77, 0x0f,  4, 312, 577,  808, 541, 143, 10,  768, 512);		// X68000  768x512 31KHz
			SetParameter<T_Preset_15>(true,  78, 0x0f, 22, 159, 614,  608, 260,  80, 25,  512, 240);		// X68000  512x240 15KHz
			SetParameter<T_Preset_15>(true,  27, 0x0f, 23, 247, 531, 1408, 465, 288, 33, 1024, 424);		// X68000 1024x424 24KHz
			SetParameter<T_Preset_15>(true,  46, 0x0f, 27, 245, 524,  944, 469, 240, 21,  640, 448);		// X68000 Fantasy Zone 24KHz
			SetParameter<T_Preset_15>(true,  54, 0x0f, 31, 315, 543, 1178, 580, 308, 38,  768, 536);		// X68000 Dash Yarou 31KHz
			SetParameter<T_Preset_15>(true,  87, 0x0f, 35, 315, 529, 1104, 595, 360, 32,  672, 560);		// X68000 Druaga 31KHz  
			SetParameter<T_Preset_15>(true,  99, 0x1f, 37, 247, 531, 1408, 930, 288, 33, 1020, 848, 44);	// X68000 1024x848 24KHz(interlace) 偶数・奇数ライン逆？
			SetParameter<T_Preset_15>(true, 101, 0x1f, 38, 159, 613,  608, 525,  80, 25,  516, 480, 4);		// X68000  512x512 15KHz(interlace)
			SetParameter<T_Preset_15>(true, 117, 0x0f, 42, 247, 556,  848, 440,  85, 24,  640, 400, 4);		// PC-98 24KHz
		}
		else if (mode == P2421H) {
			// あまり後ろの方のtblは使わない方がよさそう
			SetParameter<T_Preset_15>(true,  77, 0x0f,  4, 315, 555, 1104, 567, 263, 41,  768, 512, 4);		// X68000  768x512 31KHz
			SetParameter<T_Preset_15>(true, 128, 0x0f, 46, 312, 577,  808, 541, 143, 10,  768, 512, 4);		// X68000  768x512 31KHz
			SetParameter<T_Preset_15>(true,  78, 0x0f, 22, 159, 614,  608, 260,  80, 25,  512, 240, 4);		// X68000  512x240 15KHz
			SetParameter<T_Preset_15>(true,  11, 0x0f,  6, 247, 531, 1408, 465, 288, 33, 1024, 424, 4);		// X68000 1024x424 24KHz
			SetParameter<T_Preset_15>(true,  46, 0x0f, 27, 245, 524,  944, 469, 240, 21,  640, 448, 4);		// X68000 Fantasy Zone 24KHz
			SetParameter<T_Preset_15>(true,  54, 0x0f, 31, 315, 543, 1178, 580, 308, 38,  768, 536, 4);		// X68000 Dash Yarou 31KHz
			SetParameter<T_Preset_15>(true,  87, 0x0f, 35, 315, 529, 1104, 595, 360, 32,  672, 560, 4);		// X68000 Druaga 31KHz  
			SetParameter<T_Preset_15>(true, 100, 0x1f, 37, 247, 531, 1408, 930, 288, 33, 1020, 848, 44);	// X68000 1024x848 24KHz(interlace) 偶数・奇数ライン逆？
			SetParameter<T_Preset_15>(true,   1, 0x1f,  0, 159, 613,  608, 525,  80, 25,  516, 480, 4);		// X68000  512x512 15KHz(interlace)
			SetParameter<T_Preset_15>(true,  67, 0x0f, 11, 247, 556,  848, 440,  85, 24,  640, 400, 4);		// PC-98 24KHz
		}

		strcat(szFname, "_mod");
		_makepath(szPath, szDrive, szDir, szFname, szExt);
		fp = fopen(szPath, "wb");
		if (!fp) {
			printf("can't write %s\n", szPath);
			return RC_WRITE_ERROR;
		}
		fwrite(buf, file_len, 1, fp);
		fclose(fp);
	}

	// width height table
	printf("offset  "_SEP_"tbl"_SEP_"widt"_SEP_"heig\n");
	for (int i=0; i<wh_num; i++) {
		T_WH wh;
		int pos = wh_ofs+i*sizeof(wh);
		memmove(&wh, &buf[pos], sizeof(wh));
		if (IsBigEndian(mode)) {
			wh.width = htons(wh.width);
			wh.height = htons(wh.height);
		}
		printf("0x%06x"_SEP_"%3d"_SEP_"%4d"_SEP_"%4d\n", pos, i, wh.width, wh.height);
	}

	// preset table
	char szAddHeader[32];
	if (mode == TV56031 || mode == KP1208IPS) {		// T_Preset_24
		strcpy(szAddHeader, "");
	}
	else if (mode == BNC59H) {				// T_Preset_20_2
		strcpy(szAddHeader, "");
	}
	else if (mode == EA245WMi) {			// T_Preset_20
		strcpy(szAddHeader, "");
	}
	else if (mode == MX279H) {				// T_Preset_15
		strcpy(szAddHeader, "");
	}
	else {									// T_Preset_14
		strcpy(szAddHeader, _SEP_"nazo");
	}
	printf("offset  "_SEP_"tbl"_SEP_"polf"_SEP_"idx"_SEP_"widt"_SEP_"heig"_SEP_"hFrq"_SEP_"vFrq"_SEP_"hTot"_SEP_"vTot"_SEP_"hSBP"_SEP_"vSBP%s\n", szAddHeader);
	for (int i=0; i<tbl_num; i++) {
		if (mode == TV56031 || mode == KP1208IPS) {
			DumpPreset<T_Preset_24>(i, false);
		}
		else if (mode == BNC59H) {
			DumpPreset<T_Preset_20_2>(i, false);
		}
		else if (mode == EA245WMi) {
			DumpPreset<T_Preset_20>(i, false);
		}
		else if (mode == MX279H) {
			DumpPreset<T_Preset_14>(i, true);
		}
		else {
			DumpPreset<T_Preset_15>(i, true);
		}
	}

	return RC_SUCCESS;
}

