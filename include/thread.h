#ifndef AK_THREAD__H
#define AK_THREAD__H

#include <pthread.h>

namespace noos {

	// TODO: implement an option to provide attributes

	class thread {
		public:
			thread();
			virtual ~thread();
			void start();
			void join();

		protected:
			virtual void run() = 0;
			void exit();
			void detached_exit();

		private:
			static void * run_thread(thread * p);
			pthread_t pt;
	};

}


#endif
