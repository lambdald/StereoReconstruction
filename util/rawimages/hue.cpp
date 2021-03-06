//---------------------------------------------------------------------
//
// Copyright © 2011, Jason Gedge <gedge -at- ualberta -dot- ca>
//
// This file is part of StereoReconstruction.
//
// StereoReconstruction is free software: you can redistribute it and/or
// modify it under the terms of the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// StereoReconstruction is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with StereoReconstruction. If not, see <http:www.gnu.org/licenses/>.
//
//---------------------------------------------------------------------
#include "rawimagereader.hpp"

using std::log;

//! Smooth hue transition.
unsigned char * rawImageToRGB_hue(const unsigned char *data, unsigned char *out, int w, int h) {
	//
	// First pass; bilinear interpolation for green pixels
	//
	for(int y = 0; y < h; ++y) {
		for(int x = 0; x < w; ++x) {
			const int maskShift = (((x + 1) % 2) + 2*((y + 1) % 2)) * 8;
			const int maskVal = (MASK & (0xFF << maskShift)) >> maskShift;

			switch(maskVal) {
				case 'R':
				case 'B':
				{
					int counter = 0;
					int sum = 0;
					GET_PIXEL(data, sum, x, y-1, w, h, counter);
					GET_PIXEL(data, sum, x, y+1, w, h, counter);
					GET_PIXEL(data, sum, x-1, y, w, h, counter);
					GET_PIXEL(data, sum, x+1, y, w, h, counter);
					out[3*(y*w + x) + 1] = (counter == 0 ? 0 : sum / counter);
					break;
				}
				case 'G':
					out[3*(y*w + x) + 1] = data[y*w + x];
					break;
			}
		}
	}

	//
	// Second pass; smooth hue transition for Red/Blue pixels
	//
	for(int y = 0; y < h; ++y) {
		for(int x = 0; x < w; ++x) {
			const int maskShift = (((x + 1) % 2) + 2*((y + 1) % 2)) * 8;
			const int maskVal = (MASK & (0xFF << maskShift)) >> maskShift;
			const int value = data[y*w + x];

			switch(maskVal) {
				case 'G':
				{
					int pn_exists = 0, pe_exists = 0, ps_exists = 0, pw_exists = 0;
					int pn = 0, pe = 0, ps = 0, pw = 0;
					int gn = 0, ge = 0, gs = 0, gw = 0;

					GET_PIXEL(data, pn, x, y-1, w, h, pn_exists);
					GET_PIXEL(data, ps, x, y+1, w, h, ps_exists);
					GET_PIXEL(data, pw, x-1, y, w, h, pw_exists);
					GET_PIXEL(data, pe, x+1, y, w, h, pe_exists);

					GET_GREEN_PIXEL(out, gn, x, y-1, w, h, pn_exists);
					GET_GREEN_PIXEL(out, gs, x, y+1, w, h, ps_exists);
					GET_GREEN_PIXEL(out, gw, x-1, y, w, h, pw_exists);
					GET_GREEN_PIXEL(out, ge, x+1, y, w, h, pe_exists);

					// Check boundary conditions
					int val_ns = (pn_exists ? pn : ps);
					if(pn_exists && ps_exists) {
						if(gn == 0 || gs == 0) {
							val_ns = 0;
						} else {
							val_ns = (value*(gs*pn + gn*ps)) / (2*gn*gs);
							if(val_ns > 255)
								val_ns = 255;
						}
					}

					int val_ew = (pe_exists ? pe : pw);
					if(pe_exists && pw_exists) {
						if(ge == 0 || gw == 0) {
							val_ew = 0;
						} else {
							val_ew = (value*(ge*pw + gw*pe)) / (2*ge*gw);
							if(val_ew > 255)
								val_ew = 255;
						}
					}

					//
					if(y % 2 == 0) {
						out[3*(y*w + x) + 0] = val_ew;
						out[3*(y*w + x) + 2] = val_ns;
					} else {
						out[3*(y*w + x) + 0] = val_ns;
						out[3*(y*w + x) + 2] = val_ew;
					}

					break;
				}
				case 'R':
				case 'B':
				{
					int counter = 0;
					int pn = 0, pe = 0, ps = 0, pw = 0;
					int gn = 0, ge = 0, gs = 0, gw = 0, green = 0;
					int pn_exists = 0, pe_exists = 0, ps_exists = 0, pw_exists = 0;

					GET_PIXEL(data, pn, x-1, y-1, w, h, counter);
					GET_PIXEL(data, pe, x-1, y+1, w, h, counter);
					GET_PIXEL(data, ps, x+1, y-1, w, h, counter);
					GET_PIXEL(data, pw, x+1, y+1, w, h, counter);

					GET_GREEN_PIXEL(out, green, x, y, w, h, pn_exists);
					pn_exists = 0;
					GET_GREEN_PIXEL(out, gn, x-1, y-1, w, h, pn_exists);
					GET_GREEN_PIXEL(out, ge, x-1, y+1, w, h, pe_exists);
					GET_GREEN_PIXEL(out, gs, x+1, y-1, w, h, ps_exists);
					GET_GREEN_PIXEL(out, gw, x+1, y+1, w, h, pw_exists);

					//
					double smooth = 0;
					if(pn_exists > 0) smooth += log(pn == 0 ? 0.1 : pn) - log(gn == 0 ? 0.1 : gn);
					if(pe_exists > 0) smooth += log(pe == 0 ? 0.1 : pe) - log(ge == 0 ? 0.1 : ge);
					if(ps_exists > 0) smooth += log(ps == 0 ? 0.1 : ps) - log(gs == 0 ? 0.1 : gs);
					if(pw_exists > 0) smooth += log(pw == 0 ? 0.1 : pw) - log(gw == 0 ? 0.1 : gw);
					smooth = exp(log(1.0*green) + (1.0 / counter)*smooth);

					if(smooth > 255)
						smooth = 255;

					if(y % 2 == 0) {
						out[3*(y*w + x) + 0] = value;
						out[3*(y*w + x) + 2] = (unsigned char)smooth;
					} else {
						out[3*(y*w + x) + 0] = (unsigned char)smooth;
						out[3*(y*w + x) + 2] = value;
					}

					break;
				}
			}
		}
	}

	return out;
}
