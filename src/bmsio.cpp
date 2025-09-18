/*
 * This file is part of the FlyingADCBMS project.
 *
 * Copyright (C) 2025 Johannes Huebner <dev@johanneshuebner.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "bmsio.h"
#include "params.h"
#include "anain.h"
#include "temp_meas.h"
#include "my_math.h"
#include "flyingadcbms.h"
#include "maxbms.h"
#include "bmsalgo.h"
#include "cellmux.h"
#include "mcp3421.h"

// BmsHardware bmshardware = FlyingAdcBms();
// FlyingAdcBms bmshardware = FlyingAdcBms();

#ifdef HW_FLYING_ADC_V1
    #include "flyingadcbms.h"
    BmsHardware BmsIO::bmshardware = FlyingAdcBms();
#elif HW_FLYING_ADC_V2
    #include "flyingadcbms.h"
    BmsHardware BmsIO::bmshardware = FlyingAdcBms();
#elif HW_MAX17841B
    #include "maxbms.h"
    BmsHardware BmsIO::bmshardware = MaxBms();
#else
    #error "Unknown hardware variant"
#endif

BmsFsm* BmsIO::bmsFsm;
// int BmsIO::muxRequest = -1;

void BmsIO::Init() 
{
   bmshardware.Init();
}

void BmsIO::ReadCellVoltages()
{
   bmshardware.Ms25Task();
}

void BmsIO::ReadTemperatures()
{
   int sensor = Param::GetInt(Param::tempsns);
   int nomRes = Param::GetInt(Param::tempres);
   int beta = Param::GetInt(Param::tempbeta);
   float temp1 = NO_TEMP, temp2 = NO_TEMP, tempmin = NO_TEMP, tempmax = NO_TEMP;

   if (sensor & 1)
      tempmin = tempmax = temp1 = TempMeas::AdcToTemperature(AnaIn::temp1.Get(), nomRes, beta);

   if (sensor & 2)
      tempmin = tempmax = temp2 = TempMeas::AdcToTemperature(AnaIn::temp2.Get(), nomRes, beta);

   if (sensor == 3) //two sensors, calculate min and max
   {
      tempmin = MIN(temp1, temp2);
      tempmax = MAX(temp1, temp2);
   }

   Param::SetFloat(Param::tempmin0, tempmin);
   Param::SetFloat(Param::tempmax0, tempmax);
}

void BmsIO::MeasureCurrent()
{
   int idcmode = Param::GetInt(Param::idcmode);

   if (idcmode == IDC_DIFFERENTIAL || idcmode == IDC_SINGLE)
   {
      float current = 0;
      static int samples = 0;
      static u32fp amsIn = 0, amsOut = 0;
      static float idcavg = 0;
      int curpos = AnaIn::curpos.Get();
      int curneg = AnaIn::curneg.Get();
      float idcgain = Param::GetFloat(Param::idcgain);
      int idcofs = Param::GetInt(Param::idcofs);
      int rawCurrent = idcmode == IDC_SINGLE ? curpos : curpos - curneg;

      current = (rawCurrent - idcofs) / idcgain;

      if (current < -Param::GetFloat(Param::idlethresh))
      {
         amsOut += -FP_FROMFLT(current);
      }
      else if (current > Param::GetFloat(Param::idlethresh))
      {
         amsIn += FP_FROMFLT(current);
      }

      idcavg += current;
      samples++;

      if (samples == 200)
      {
         s32fp chargein = Param::Get(Param::chargein);
         s32fp chargeout = Param::Get(Param::chargeout);

         chargein += amsIn / 200;
         chargeout += amsOut / 200;
         idcavg /= 200;

         float voltage = Param::GetFloat(Param::utotal) / 1000;
         float power = voltage * idcavg;

         Param::SetFloat(Param::idcavg, idcavg);
         Param::SetFloat(Param::power, power);

         amsIn = 0;
         amsOut = 0;
         samples = 0;
         idcavg = 0;

         Param::SetFixed(Param::chargein, chargein);
         Param::SetFixed(Param::chargeout, chargeout);
      }
      Param::SetFloat(Param::idc, current);
   }
}

void BmsIO::TestReadCellVoltage(int chan, BmsAlgo::BalanceCommand cmd)
{
   // float gain = Param::GetFloat(Param::gain);

   // if (chan == 0)
   //    gain *= 1 + Param::GetFloat(Param::correction0) / 1000000.0f;
   // else if (chan == 1)
   //    gain *= 1 + Param::GetFloat(Param::correction1) / 1000000.0f;
   // else if (chan == 15)
   //    gain *= 1 + Param::GetFloat(Param::correction15) / 1000000.0f;

   // float udc = CellMux::GetResult() * (gain / 1000.0f);;
   // CellMux::SelectChannel(chan);
   // bmshardware.SetBalancing(cmd);
   // CellMux::StartAdc();
   // Param::SetFloat((Param::PARAM_NUM)(Param::u0 + chan), udc);
}

void BmsIO::Accumulate(float sum, float min, float max, float avg)
{
   if (bmsFsm->IsFirst())
   {
      Param::SetFloat(Param::uavg0, avg);
      Param::SetFloat(Param::umin0, min);
      Param::SetFloat(Param::umax0, max);

      float totalSum = sum, totalMin = min, totalMax = max;
      //If we are the first module accumulate our values with those from the sub modules
      for (int i = 1; i < bmsFsm->GetNumberOfModules(); i++)
      {
         //Here we undo the local average calculation on the module to calculate the substrings total voltage
         totalSum += Param::GetFloat(bmsFsm->GetDataItem(Param::uavg0, i)) * bmsFsm->GetCellsOfModule(i);
         totalMin = MIN(totalMin, Param::GetFloat(bmsFsm->GetDataItem(Param::umin0, i)));
         totalMax = MAX(totalMax, Param::GetFloat(bmsFsm->GetDataItem(Param::umax0, i)));
      }

      float tempmin = NO_TEMP, tempmax = -40;

      for (int i = 0; i < bmsFsm->GetNumberOfModules(); i++)
      {
         float tempmin0 = Param::GetFloat(bmsFsm->GetDataItem(Param::tempmin0, i));
         float tempmax0 = Param::GetFloat(bmsFsm->GetDataItem(Param::tempmax0, i));

         if (tempmin0 < NO_TEMP)
         {
            tempmin = MIN(tempmin, tempmin0);
            tempmax = MAX(tempmax, tempmax0);
         }
      }

      Param::SetFloat(Param::umin, totalMin);
      Param::SetFloat(Param::umax, totalMax);
      Param::SetFloat(Param::uavg, totalSum / Param::GetInt(Param::totalcells));
      Param::SetFloat(Param::udelta, totalMax - totalMin);
      Param::SetFloat(Param::utotal, totalSum);
      Param::SetFloat(Param::tempmin, tempmin);
      Param::SetFloat(Param::tempmax, tempmax);
   }
   else //if we are a sub module write averages straight to data module
   {
      Param::SetFloat(Param::utotal, sum);
      Param::SetFloat(Param::uavg0, avg);
      Param::SetFloat(Param::umin0, min);
      Param::SetFloat(Param::umax0, max);
      Param::SetFloat(Param::udelta, max - min);
   }
}
