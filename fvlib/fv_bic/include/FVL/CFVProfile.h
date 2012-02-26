#pragma once
#define _H_CPROFILE

#include <cuda.h>
#include <cuda_runtime.h>

#include "FVL/FVGlobal.h"
#include "FVL/FVLog.h"

namespace FVL {

	class CFVProfile {
		private:
		string msg;

		public:
		static FVLog stream; //TODO por esta coisa privada
		cudaEvent_t start_t, stop_t;
		float time;

		CFVProfile(string msg);
		CFVProfile(string msg, string filename);
		~CFVProfile();

		void start();
		void stop();
		float getTime();

		private:
		void init(string msg, string filename);
	};
}