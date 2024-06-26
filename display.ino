#include <fonts/FreeSans9pt7b.h>
// TFT display constants
#define PORTRAIT 		0
#define LANDSCAPE 		1

#define TFT_WIDTH		320
#define TFT_HEIGHT		240
#define GRID_WIDTH		300
#define GRID_HEIGHT		200

#define GRID_COLOR		0x630c    //0x4208
#define ADC_MAX_VAL		4096
//#define ADC_2_GRID    4468    // 0.5v/div =0.5*4096*8/3.3*0.9

const uint16_t voltageDiv[] = {
  44684,  // 5v/div =5.0*4096*8/3.3*0.9
  17873,  // 2v/div =2.0*4096*8/3.3*0.9
  8937,   // 1v/div =1.0*4096*8/3.3*0.9
  4468,   // 0.5v/div =0.5*4096*8/3.3*0.9
  1787,   // 0.2v/div
  894,    // 0.1v/div
  447,    // 50mv/div
  197,    // 20mv/div
  89};    // 10mv/div

uint16_t adc_2_grid1, adc_2_grid2;

#define TFT_CS         PA15
#define TFT_DC         PB10
#define TFT_RST        PB11
Adafruit_ILI9341_STM tft = Adafruit_ILI9341_STM(TFT_CS, TFT_DC, TFT_RST); // Mosi - PA7, SCK - PA5

// rendered waveform data is stored here for erasing
int16_t ch1Old[GRID_WIDTH] = {0};
int16_t ch2Old[GRID_WIDTH] = {0};
int8_t bitOld[GRID_WIDTH] = {0};

// grid variables
uint8_t hOffset = (TFT_WIDTH - GRID_WIDTH)/2;
uint8_t vOffset = (TFT_HEIGHT - GRID_HEIGHT)/2;
uint8_t dHeight = GRID_HEIGHT/8;

// plot variables -- modified by interface section
// controls which section of waveform is displayed on screen
// 0 < xCursor < (NUM_SAMPLES - GRID_WIDTH)
int16_t xCursor;
// controls the vertical positioning of waveform
int16_t yCursors[4];
// controls which waveforms are displayed
boolean waves[4];
// prints waveform statistics on screen
boolean printStats = true;
// repaint the labels on screen in draw loop
boolean paintLabels = false;

// labels around the grid
enum {L_timebase, L_triggerType, L_triggerEdge, L_triggerLevel, L_waves, L_window, L_vPos1, L_vPos2, L_vPos3, L_vPos4, L_range1, L_range2};
uint8_t currentFocus = L_timebase;


// ------------------------
void focusNextLabel()	{
// ------------------------
	currentFocus++;

	if((currentFocus == L_vPos1) && !waves[0])
		currentFocus++;

	if((currentFocus == L_vPos2) && !waves[1])
		currentFocus++;

	if((currentFocus == L_vPos3) && !waves[2])
		currentFocus++;

	if((currentFocus == L_vPos4) && !waves[3])
		currentFocus++;

  if((currentFocus == L_range2) && !waves[1])
    currentFocus++;

	if(currentFocus > L_range2)
		currentFocus = L_timebase;
}




// ------------------------
void repaintLabels()	{
// ------------------------
	paintLabels = true;
}



// ------------------------
void initDisplay()	{
// ------------------------
	tft.begin();
	tft.setRotation(LANDSCAPE);
	tft.fillScreen(ILI9341_BLACK);
	banner();
  tft.setFont(&FreeSans9pt7b);

	delay(1000);

	// and paint o-scope
	clearWaves();
}




// ------------------------
void drawWaves()	{
// ------------------------
	static boolean printStatsOld = false;

	if(printStatsOld && !printStats)
		clearStats();
	
	printStatsOld = printStats;

	// draw the grid
	drawGrid();
	
	// clear and draw signal traces
	clearNDrawSignals();
	
	// if requested update the stats
	if(printStats)
		drawStats();

	// if label repaint requested - do so now
	if(paintLabels)	{
		drawLabels();
		paintLabels = false;
	}
}




// ------------------------
void clearWaves()	{
// ------------------------
	// clear screen
	tft.fillScreen(ILI9341_BLACK);
	// and paint o-scope
	drawGrid();
	drawLabels();
}



boolean cDisplayed = false;

// ------------------------
void indicateCapturing()	{
// ------------------------
	if((currentTimeBase > T2MS) || (triggerType != TRIGGER_AUTO))	{
		cDisplayed = true;
    tft.setFont(NULL);
		tft.setTextColor(ILI9341_PINK, ILI9341_BLACK);
		tft.setCursor(140, 20);
		tft.print("Sampling...");
    tft.setFont(&FreeSans9pt7b);
	}
}



// ------------------------
void indicateCapturingDone()	{
// ------------------------
	if(cDisplayed)	{
		tft.fillRect(140, 20, 66, 8, ILI9341_BLACK);
		cDisplayed = false;
	}
}





// local operations below




// 0, 1 Analog channels. 2, 3 digital channels
// ------------------------
void clearNDrawSignals()	{
// ------------------------
	static boolean wavesOld[4] = {false,};
	static int16_t yCursorsOld[4];
  static uint8_t rangePos1Old, rangePos2Old;

  if (rangePos1 != rangePos1Old || rangePos2 != rangePos2Old)
    clearWaves();
  rangePos1Old = rangePos1;
  rangePos2Old = rangePos2;
  adc_2_grid1 = voltageDiv[rangePos1];
  adc_2_grid2 = voltageDiv[rangePos2];
	// snap the values to prevent interrupt from changing mid-draw
	int16_t xCursorSnap = xCursor;
	int16_t zeroVoltageA1Snap = zeroVoltageA1;
	int16_t zeroVoltageA2Snap = zeroVoltageA2;
	int16_t yCursorsSnap[4];
	boolean wavesSnap[4];
	yCursorsSnap[0] = yCursors[0];
	yCursorsSnap[1] = yCursors[1];
	yCursorsSnap[2] = yCursors[2];
	yCursorsSnap[3] = yCursors[3];
	wavesSnap[0] = waves[0];
	wavesSnap[1] = waves[1];
	wavesSnap[2] = waves[2];
	wavesSnap[3] = waves[3];

  if (couplingPos1 == CPL_DC) {
    zeroVoltageA1Snap = 0;
    yCursorsSnap[0] = yCursors[0] + GRID_HEIGHT / 2;
  }
  if (couplingPos2 == CPL_DC) {
    zeroVoltageA2Snap = 0;
    yCursorsSnap[1] = yCursors[1] + GRID_HEIGHT / 2;
  }

	// draw the GRID_WIDTH section of the waveform from xCursorSnap
	int16_t val1, val2;
	int16_t transposedPt1, transposedPt2;
	uint8_t shiftedVal;

	// sampling stopped at sIndex - 1
	int j = sIndex + xCursorSnap;
	if(j >= NUM_SAMPLES)
		j = j - NUM_SAMPLES;
	
	// go through all the data points
	for(int i = 1, jn = j + 1; i < GRID_WIDTH - 1; j++, i++, jn++)	{
		if(jn == NUM_SAMPLES)
			jn = 0;

		if(j == NUM_SAMPLES)
			j = 0;

		// erase old line segment
		if(wavesOld[3])	{
			val1 = (bitOld[i] & 0b10000000) ? dHeight : 0;
			val2 = (bitOld[i + 1] & 0b10000000) ? dHeight : 0;
			// clear the line segment
			transposedPt1 = GRID_HEIGHT + vOffset + yCursorsOld[3] - val1;
			transposedPt2 = GRID_HEIGHT + vOffset + yCursorsOld[3] - val2;
			plotLineSegment(transposedPt1, transposedPt2, i, ILI9341_BLACK);
		}

		if(wavesOld[2])	{
			val1 = (bitOld[i] & 0b01000000) ? dHeight : 0;
			val2 = (bitOld[i + 1] & 0b01000000) ? dHeight : 0;
			// clear the line segment
			transposedPt1 = GRID_HEIGHT + vOffset + yCursorsOld[2] - val1;
			transposedPt2 = GRID_HEIGHT + vOffset + yCursorsOld[2] - val2;
			plotLineSegment(transposedPt1, transposedPt2, i, ILI9341_BLACK);
		}
			
		if(wavesOld[1])	{
			val1 = (ch2Old[i] * GRID_HEIGHT)/adc_2_grid2;
			val2 = (ch2Old[i + 1] * GRID_HEIGHT)/adc_2_grid2;
			// clear the line segment
			transposedPt1 = GRID_HEIGHT + vOffset + yCursorsOld[1] - val1;
			transposedPt2 = GRID_HEIGHT + vOffset + yCursorsOld[1] - val2;
			plotLineSegment(transposedPt1, transposedPt2, i, ILI9341_BLACK);
		}

		if(wavesOld[0])	{
			val1 = (ch1Old[i] * GRID_HEIGHT)/adc_2_grid1;
			val2 = (ch1Old[i + 1] * GRID_HEIGHT)/adc_2_grid1;
			// clear the line segment
			transposedPt1 = GRID_HEIGHT + vOffset + yCursorsOld[0] - val1;
			transposedPt2 = GRID_HEIGHT + vOffset + yCursorsOld[0] - val2;
			plotLineSegment(transposedPt1, transposedPt2, i, ILI9341_BLACK);
		}
			
		// draw new segments
		if(wavesSnap[3])	{
			shiftedVal = bitStore[j] >> 7;
			val1 = (shiftedVal & 0b10000000) ? dHeight : 0;
			val2 = ((bitStore[jn] >> 7) & 0b10000000) ? dHeight : 0;
			bitOld[i] &= 0b01000000;
			bitOld[i] |= shiftedVal & 0b10000000;
			// draw the line segment
			transposedPt1 = GRID_HEIGHT + vOffset + yCursorsSnap[3] - val1;
			transposedPt2 = GRID_HEIGHT + vOffset + yCursorsSnap[3] - val2;
			plotLineSegment(transposedPt1, transposedPt2, i, DG_SIGNAL2);
		}
		
		if(wavesSnap[2])	{
			shiftedVal = bitStore[j] >> 7;
			val1 = (shiftedVal & 0b01000000) ? dHeight : 0;
			val2 = ((bitStore[jn] >> 7) & 0b01000000) ? dHeight : 0;
			bitOld[i] &= 0b10000000;
			bitOld[i] |= shiftedVal & 0b01000000;
			// draw the line segment
			transposedPt1 = GRID_HEIGHT + vOffset + yCursorsSnap[2] - val1;
			transposedPt2 = GRID_HEIGHT + vOffset + yCursorsSnap[2] - val2;
			plotLineSegment(transposedPt1, transposedPt2, i, DG_SIGNAL1);
		}

		if(wavesSnap[1])	{
			val1 = ((ch2Capture[j] - zeroVoltageA2Snap) * GRID_HEIGHT)/adc_2_grid2;
			val2 = ((ch2Capture[jn] - zeroVoltageA2Snap) * GRID_HEIGHT)/adc_2_grid2;
			ch2Old[i] = ch2Capture[j] - zeroVoltageA2Snap;
			// draw the line segment
			transposedPt1 = GRID_HEIGHT + vOffset + yCursorsSnap[1] - val1;
			transposedPt2 = GRID_HEIGHT + vOffset + yCursorsSnap[1] - val2;
			plotLineSegment(transposedPt1, transposedPt2, i, AN_SIGNAL2);
		}
		
		if(wavesSnap[0])	{
			val1 = ((ch1Capture[j] - zeroVoltageA1Snap) * GRID_HEIGHT)/adc_2_grid1;
			val2 = ((ch1Capture[jn] - zeroVoltageA1Snap) * GRID_HEIGHT)/adc_2_grid1;
			ch1Old[i] = ch1Capture[j] - zeroVoltageA1Snap;
			// draw the line segment
			transposedPt1 = GRID_HEIGHT + vOffset + yCursorsSnap[0] - val1;
			transposedPt2 = GRID_HEIGHT + vOffset + yCursorsSnap[0] - val2;
			plotLineSegment(transposedPt1, transposedPt2, i, AN_SIGNAL1);
		}

	}
	
	
	// store the drawn parameters to old storage
	wavesOld[0] = wavesSnap[0];
	wavesOld[1] = wavesSnap[1];
	wavesOld[2] = wavesSnap[2];
	wavesOld[3] = wavesSnap[3];
	
	yCursorsOld[0] = yCursorsSnap[0];
	yCursorsOld[1] = yCursorsSnap[1];
	yCursorsOld[2] = yCursorsSnap[2];
	yCursorsOld[3] = yCursorsSnap[3];

}




// ------------------------
inline void plotLineSegment(int16_t transposedPt1, int16_t transposedPt2,  int index, uint16_t color)	{
// ------------------------
	// range checks
	if(transposedPt1 > (GRID_HEIGHT + vOffset))
		transposedPt1 = GRID_HEIGHT + vOffset;
	if(transposedPt1 < vOffset)
		transposedPt1 = vOffset;
	if(transposedPt2 > (GRID_HEIGHT + vOffset))
		transposedPt2 = GRID_HEIGHT + vOffset;
	if(transposedPt2 < vOffset)
		transposedPt2 = vOffset;

	// draw the line segments
	tft.drawLine(index + hOffset, transposedPt1, index + hOffset, transposedPt2, color);
}





// ------------------------
void drawVCursor(int channel, uint16_t color, boolean highlight)	{
// ------------------------
	int cPos = GRID_HEIGHT + vOffset + yCursors[channel];
    tft.fillTriangle(0, cPos - 5, hOffset, cPos, 0, cPos + 5, color);
	if(highlight)
		tft.drawRect(0, cPos - 7, hOffset, 14, ILI9341_WHITE);
}




// ------------------------
void drawGrid()	{
// ------------------------
	uint8_t hPacing = GRID_WIDTH / 12;
	uint8_t vPacing = GRID_HEIGHT / 8;

	for(int i = 1; i < 12; i++) {
//		tft.drawFastVLine(i * hPacing + hOffset, vOffset, GRID_HEIGHT, GRID_COLOR);
    for (int y = 0; y < GRID_HEIGHT; y += 5)
      tft.drawPixel(i * hPacing + hOffset, vOffset + y, GRID_COLOR);
	}
  tft.drawFastVLine(6 * hPacing + hOffset, vOffset, GRID_HEIGHT, GRID_COLOR);

	for(int i = 1; i < 8; i++) {
//		tft.drawFastHLine(hOffset, i * vPacing + vOffset, GRID_WIDTH, GRID_COLOR);
    for (int x = 0; x < GRID_WIDTH; x += 5)
      tft.drawPixel(x + hOffset, i * vPacing + vOffset, GRID_COLOR);
	}
  tft.drawFastHLine(hOffset, 4 * vPacing + vOffset, GRID_WIDTH, GRID_COLOR);

	for(int i = 1; i < 5*8; i++)
		tft.drawFastHLine(hOffset + GRID_WIDTH/2 - 2, i * vPacing/5 + vOffset, 5, GRID_COLOR);

	for(int i = 1; i < 5*12; i++)
		tft.drawFastVLine(i * hPacing/5 + hOffset, vOffset + GRID_HEIGHT/2 - 2, 5, GRID_COLOR);

	tft.drawRect(hOffset, vOffset, GRID_WIDTH, GRID_HEIGHT + 1, ILI9341_WHITE);
}




// ------------------------
void drawLabels()	{
// ------------------------
	// draw the static labels around the grid

	// erase top bar
	tft.fillRect(hOffset, 0, TFT_WIDTH, vOffset, ILI9341_BLACK);
	tft.fillRect(hOffset + GRID_WIDTH, 0, hOffset, TFT_HEIGHT, ILI9341_BLACK);

	// paint run/hold information
	// -----------------
	tft.setCursor(hOffset + 2, 14);

	if(hold)	{
		tft.setTextColor(ILI9341_WHITE, ILI9341_RED);
		tft.print(" HOLD ");
	}
	else	{
		tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
		tft.print("RUN");
	}

	// draw x-window at top, range = 200px
	// -----------------
	int sampleSizePx = 160;
	float lOffset = (TFT_WIDTH - sampleSizePx)/2;
	tft.drawFastVLine(lOffset, 3, vOffset - 6, ILI9341_GREEN);
	tft.drawFastVLine(lOffset + sampleSizePx, 3, vOffset - 6, ILI9341_GREEN);
	tft.drawFastHLine(lOffset, vOffset/2, sampleSizePx, ILI9341_GREEN);

	// where does xCursor lie in this range
	float windowSize = GRID_WIDTH * sampleSizePx/NUM_SAMPLES;
	float xCursorPx =  xCursor * sampleSizePx/NUM_SAMPLES + lOffset;
	if(currentFocus == L_window)
		tft.drawRect(xCursorPx, 4, windowSize, vOffset - 8, ILI9341_WHITE);
	else
		tft.fillRect(xCursorPx, 4, windowSize, vOffset - 8, ILI9341_GREEN);

	
	// print active wave indicators
	// -----------------
  tft.setFont(NULL);
	tft.setCursor(250, 4);
	if(waves[0])	{
		tft.setTextColor(AN_SIGNAL1, ILI9341_BLACK);
		tft.print("A1 ");
	}
	else
		tft.print("   ");

	if(waves[1])	{
		tft.setTextColor(AN_SIGNAL2, ILI9341_BLACK);
		tft.print("A2 ");
	}
	else
		tft.print("   ");

	if(waves[2])	{
		tft.setTextColor(DG_SIGNAL1, ILI9341_BLACK);
		tft.print("D1 ");
	}
	else
		tft.print("   ");

	if(waves[3])	{
		tft.setTextColor(DG_SIGNAL2, ILI9341_BLACK);
		tft.print("D2");
	}

	if(currentFocus == L_waves)
		tft.drawRect(247, 0, 72, vOffset, ILI9341_WHITE);

	// erase left side of grid
	tft.fillRect(0, 0, hOffset, TFT_HEIGHT, ILI9341_BLACK);
	
	// draw new wave cursors
	// -----------------
	if(waves[3])
		drawVCursor(3, DG_SIGNAL2, (currentFocus == L_vPos4));
	if(waves[2])
		drawVCursor(2, DG_SIGNAL1, (currentFocus == L_vPos3));
	if(waves[1])
		drawVCursor(1, AN_SIGNAL2, (currentFocus == L_vPos2));
	if(waves[0])
		drawVCursor(0, AN_SIGNAL1, (currentFocus == L_vPos1));

	// erase bottom bar
	tft.fillRect(hOffset, GRID_HEIGHT + vOffset + 1, TFT_WIDTH, vOffset, ILI9341_BLACK);

	// print input switch pos
	// -----------------
  if(currentFocus == L_range1)
    tft.drawRect(hOffset + 0, GRID_HEIGHT + vOffset + 1, 51, vOffset-1, ILI9341_WHITE);
  tft.setFont(&FreeSans9pt7b);
	tft.setTextColor(AN_SIGNAL1, ILI9341_BLACK);
	tft.setCursor(hOffset + 2, GRID_HEIGHT + vOffset + 16);
	tft.print(rngNames[rangePos1]);
	tft.setCursor(hOffset + 51, GRID_HEIGHT + vOffset + 16);
	tft.print(cplNames[couplingPos1]);

  if(waves[1])  {
    if(currentFocus == L_range2)
      tft.drawRect(hOffset + 80, GRID_HEIGHT + vOffset + 1, 51, vOffset-1, ILI9341_WHITE);
    tft.setTextColor(AN_SIGNAL2, ILI9341_BLACK);
    tft.setCursor(hOffset + 82, GRID_HEIGHT + vOffset + 16);
    tft.print(rngNames[rangePos2]);
    tft.setCursor(hOffset + 131, GRID_HEIGHT + vOffset + 16);
    tft.print(cplNames[couplingPos2]);
  }
	
	// print new timebase
	// -----------------
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.setCursor(170, GRID_HEIGHT + vOffset + 16);
	if(currentFocus == L_timebase)
		tft.drawRect(168, GRID_HEIGHT + vOffset + 1, 62, vOffset-1, ILI9341_WHITE);
	tft.print(getTimebaseLabel());

	// print trigger type
	// -----------------
	tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
	tft.setCursor(234, GRID_HEIGHT + vOffset + 16);
	if(currentFocus == L_triggerType)
		tft.drawRect(230, GRID_HEIGHT + vOffset + 1, 60, vOffset-1, ILI9341_WHITE);

	switch(triggerType)	{
		case TRIGGER_AUTO:
			tft.print("AUTO");
			break;
		case TRIGGER_NORM:
			tft.print("NORM");
			break;
		case TRIGGER_SINGLE:
			tft.print("SING");
			break;
	}

	// draw trigger edge
	// -----------------
	if(currentFocus == L_triggerEdge)
		tft.drawRect(294, GRID_HEIGHT + vOffset + 1, 15, vOffset - 1, ILI9341_WHITE);

	int trigX = 298;
	
	if(triggerRising)	{
		tft.drawFastHLine(trigX, TFT_HEIGHT - 3, 5, ILI9341_GREEN);
		tft.drawFastVLine(trigX + 4, TFT_HEIGHT -vOffset + 3, vOffset - 6, ILI9341_GREEN);
		tft.drawFastHLine(trigX + 4, TFT_HEIGHT -vOffset + 3, 5, ILI9341_GREEN);
		tft.fillTriangle(trigX + 2, 232, trigX + 4, 230, trigX + 6, 232, ILI9341_GREEN);
	}
	else	{
		tft.drawFastHLine(trigX + 4, TFT_HEIGHT - 3, 5, ILI9341_GREEN);
		tft.drawFastVLine(trigX + 4, TFT_HEIGHT -vOffset + 3, vOffset - 6, ILI9341_GREEN);
		tft.drawFastHLine(trigX - 1, TFT_HEIGHT -vOffset + 3, 5, ILI9341_GREEN);
		tft.fillTriangle(trigX + 2, 231, trigX + 4, 233, trigX + 6, 231, ILI9341_GREEN);
	}	
	
	
	// draw trigger level on right side
	// -----------------
  int cPos;
  if (couplingPos1 == CPL_DC)
    cPos = GRID_HEIGHT + vOffset + yCursors[0] + GRID_HEIGHT / 2 - ((getTriggerLevel()+2048)*GRID_HEIGHT)/adc_2_grid1;
  else
    cPos = GRID_HEIGHT + vOffset + yCursors[0] - (getTriggerLevel()*GRID_HEIGHT)/adc_2_grid1;
    tft.fillTriangle(TFT_WIDTH, cPos - 5, TFT_WIDTH - hOffset, cPos, TFT_WIDTH, cPos + 5, AN_SIGNAL1);
	if(currentFocus == L_triggerLevel)
		tft.drawRect(GRID_WIDTH + hOffset, cPos - 7, hOffset, 14, ILI9341_WHITE);
}


// #define DRAW_TIMEBASE

// ------------------------
void drawStats()	{
// ------------------------
	static long lastCalcTime = 0;
	boolean clearStats = false;
	
	// calculate stats once a while
	if(millis() - lastCalcTime > 300)	{
		lastCalcTime = millis();	
		calculateStats();
		clearStats = true;
	}

	// draw stat labels
	tft.setTextColor(ILI9341_RED, ILI9341_BLACK);

	tft.setCursor(12, 34);
	tft.print("Freq:");
	tft.setCursor(12, 52);
	tft.print("Cycle:");
	tft.setCursor(12, 70);
	tft.print("PW:");
	tft.setCursor(12, 88);
	tft.print("Duty:");
#ifdef DRAW_TIMEBASE
	tft.setCursor(12, 106);
	tft.print("T/div:");
#endif

	tft.setCursor(215, 34);
	tft.print("Vmax:");
	tft.setCursor(215, 52);
	tft.print("Vmin:");
	tft.setCursor(215, 70);
	tft.print("Vavr:");
	tft.setCursor(215, 88);
	tft.print("Vpp:");
	tft.setCursor(215, 106);
	tft.print("Vrms:");
	
	// print new stats
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);

	if(clearStats)
		tft.fillRect(68, 22, 88, 68, ILI9341_BLACK);
	
	if(wStats.pulseValid)	{
		tft.setCursor(68, 34);
		tft.print((int) wStats.freq);
		tft.setCursor(68, 52);
		tft.print(wStats.cycle); tft.print("ms");
		tft.setCursor(68, 70);
		tft.print(wStats.avgPW/1000); tft.print("ms");
		tft.setCursor(68, 88);
		tft.print(wStats.duty, 1); tft.print(" %");
	}
	
#ifdef DRAW_TIMEBASE
	tft.setCursor(68, 106);
	int timebase = ((double)samplingTime * 25) / NUM_SAMPLES;
	if(timebase > 10000)	{
		tft.print(timebase/1000); tft.print(" ms");
	}
	else	{
		tft.print(timebase); tft.print(" us");
	}
#endif
	
	if(clearStats)
		tft.fillRect(270, 22, GRID_WIDTH + hOffset - 270 - 1, 86, ILI9341_BLACK);
	
	drawVoltage(wStats.Vmaxf, 34, wStats.mvPos);
	drawVoltage(wStats.Vminf, 52, wStats.mvPos);
	drawVoltage(wStats.Vavrf, 70, wStats.mvPos);
	drawVoltage(wStats.Vmaxf - wStats.Vminf, 88, wStats.mvPos);
	drawVoltage(wStats.Vrmsf, 106, wStats.mvPos);
	
}




// ------------------------
void calculateStats()	{
// ------------------------
	// extract waveform stats
	int16_t Vmax = -ADC_MAX_VAL, Vmin = ADC_MAX_VAL;
	int32_t sumSamples = 0;
	int64_t sumSquares = 0;
	int32_t freqSumSamples = 0;

	for(uint16_t k = 0; k < NUM_SAMPLES; k++)	{
		int16_t val = ch1Capture[k] - zeroVoltageA1;
		if(Vmax < val)
			Vmax = val;
		if(Vmin > val)
			Vmin = val;

		sumSamples += val;
		freqSumSamples += ch1Capture[k];
		sumSquares += (val * val);
	}

	// find out frequency
	uint16_t fVavr = freqSumSamples/NUM_SAMPLES;
	boolean dnWave = (ch1Capture[sIndex] < fVavr - 10);
	boolean firstOne = true;
	uint16_t cHigh = 0;

	uint16_t sumCW = 0;
	uint16_t sumPW = 0;
	uint16_t numCycles = 0;
	uint16_t numHCycles = 0;

	// sampling stopped at sIndex - 1
	for(uint16_t sCtr = 0, k = sIndex; sCtr < NUM_SAMPLES; sCtr++, k++)	{
		if(k == NUM_SAMPLES)
			k = 0;

		// mark the points where wave transitions the average value
		if(dnWave && (ch1Capture[k] > fVavr + 10))	{
			if(!firstOne)	{
				sumCW += (sCtr - cHigh);
				numCycles++;
			}
			else
				firstOne = false;

			dnWave = false;
			cHigh = sCtr;
		}

		if(!dnWave && (ch1Capture[k] < fVavr - 10))	{
			if(!firstOne)	{
				sumPW += (sCtr - cHigh);
				numHCycles++;
			}

			dnWave = true;
		}
	}

	double tPerSample = ((double)samplingTime) / NUM_SAMPLES;
	float timePerDiv = tPerSample * 25;
	double avgCycleWidth = sumCW * tPerSample / numCycles;
	
	wStats.avgPW = sumPW * tPerSample / numHCycles;
	wStats.duty = wStats.avgPW * 100 / avgCycleWidth;
	wStats.freq = 1000000/avgCycleWidth;
	wStats.cycle = avgCycleWidth/1000;
	wStats.pulseValid = (avgCycleWidth != 0) && (wStats.avgPW != 0) && ((Vmax - Vmin) > 20);
	
	wStats.mvPos = (rangePos1 == RNG_50mV) || (rangePos1 == RNG_20mV) || (rangePos1 == RNG_10mV);
	wStats.Vrmsf = sqrt(sumSquares/NUM_SAMPLES) * adcMultiplier[rangePos1];
  if (couplingPos1 == CPL_DC) {
  wStats.Vavrf = (sumSamples/NUM_SAMPLES + zeroVoltageA1) * adcMultiplier[rangePos1];
  wStats.Vmaxf = (Vmax + zeroVoltageA1) * adcMultiplier[rangePos1];
  wStats.Vminf = (Vmin + zeroVoltageA1) * adcMultiplier[rangePos1];
  } else {
	wStats.Vavrf = sumSamples/NUM_SAMPLES * adcMultiplier[rangePos1];
	wStats.Vmaxf = Vmax * adcMultiplier[rangePos1];
	wStats.Vminf = Vmin * adcMultiplier[rangePos1];
  }
}




// ------------------------
void drawVoltage(float volt, int y, boolean mvRange)	{
// ------------------------
	// text is standard 5 px wide
	int numDigits = 1;
	int lVolt = volt;
	
	// is there a negative sign at front
	if(volt < 0)	{
		numDigits++;
		lVolt = -lVolt;
	}
	
	// how many digits before 0
	if(lVolt > 999)
		numDigits++;
	if(lVolt > 99)
		numDigits++;
	if(lVolt > 9)
		numDigits++;
	
	// mv range has mV appended at back
	if(mvRange)	{
		numDigits += 1;
		int x = GRID_WIDTH + hOffset - 20 - numDigits * 5;
		tft.setCursor(x, y);
		int iVolt = volt;
		tft.print(iVolt);
		tft.print("m");
	}
	else	{
		// non mV range has two decimal pos and V appended at back
		numDigits += 3;
		int x = GRID_WIDTH + hOffset -20 - numDigits * 5;
		tft.setCursor(x, y);
		tft.print(volt);
	}

}





// ------------------------
void clearStats()	{
// ------------------------
	tft.fillRect(hOffset, vOffset, GRID_WIDTH, 88, ILI9341_BLACK);
}



// ------------------------
void banner()	{
// ------------------------
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.setTextSize(2);
	tft.setCursor(110, 30);
	tft.print("DLO-138");
	tft.drawRect(100, 25, 100, 25, ILI9341_WHITE);

	tft.setTextSize(1);
	tft.setCursor(30, 70);
	tft.print("Dual Channel O-Scope with logic analyzer");

	tft.setCursor(30, 95);
	tft.print("Usage: ");
	tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
	tft.print("https://github.com/ardyesp/DLO-138");

	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.setCursor(30, 120);
	tft.print("for STM32F103C8T6 Blue Pill and SPI TFT");
	
	tft.setCursor(30, 145);
	tft.print("Firmware version: ");
	tft.print(FIRMWARE_VERSION);

	tft.setTextSize(1);
	tft.setCursor(30, 200);
	tft.print("GNU GENERAL PUBLIC LICENSE Version 3");
}
