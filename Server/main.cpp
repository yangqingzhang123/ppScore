#include "http_server.hpp"
#include "query_processor.hpp"
#include <malloc.h>
#include <signal.h>


void sigterm_handler(int signo) {}
void sigint_handler(int signo) {}

int main(int argc, char* argv[])
{
    //初始化配置文件变量, filename,keyname
	mallopt(M_MMAP_THRESHOLD, 64*1024);
	const char *config_filename = argv[1];

	close(STDIN_FILENO);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, &sigterm_handler);
	signal(SIGINT, &sigint_handler);

    //服务器启动
	_INFO("[Server starting...] ");
	{
		int ret;
        MJ::Configuration config;
		if (config.open(config_filename))
		{
			_ERROR("open configure file < %s : %s >error",config_filename);
			return -1;
		}
        else
        {
            _INFO("listenPort:%d",config.m_listenPort);
            _INFO("threadStackSize:%d",config.m_threadStackSize);
            _INFO("receiverNum:%d",config.m_receiverNum);
            _INFO("processorNum:%d",config.m_processorNum);
            _INFO("commandLengthWarning:%d",config.m_commandLengthWarning);
            _INFO("m_dataPath:%s",config.m_dataPath.c_str());
        }

		Http_Server htp;

		Query_Processor processor;

		pthread_barrier_t processor_init;
		pthread_barrier_init(&processor_init, NULL, config.m_processorNum + 1);

		processor.register_httpserver(&htp);
		htp.register_processor(&processor);

		if((ret = processor.open(config.m_processorNum,config.m_threadStackSize,&processor_init,config.m_dataPath)) < 0)
		{
			_ERROR("open processor error! ret:%d,thread:%d",ret,config.m_processorNum);
			exit(-1);
		}
		if((ret = htp.open(config.m_receiverNum,config.m_threadStackSize,config.m_listenPort)) <0 )
		{
			_ERROR("open http server error! ret:%d,thread:%d,listen port:%d",ret,config.m_receiverNum,config.m_listenPort);
			exit(-1);
		}

		_INFO("Server initialized OK");
		processor.activate();
		pthread_barrier_wait(&processor_init);	// wait for all processors fully initialized
		//WEBSEARCH_DEBUG((LM_DEBUG,"processor activate\n"));
		sleep(1);
		htp.activate();
		//WEBSEARCH_DEBUG((LM_DEBUG,"http activate\n"));
		_INFO("[Server started]");

		pause();
		htp.stop();
		processor.stop();
		pthread_barrier_destroy(&processor_init);
	}
	_INFO("[Server stop]");

#ifdef WRITE_CERR_LOG_INTO_FILE
	file.close();
#endif
	return 0;
}

