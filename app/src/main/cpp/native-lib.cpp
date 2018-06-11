#include <jni.h>
#include <string>
#include <pthread.h>
#include <unistd.h>
#include <android/log.h>
#include <stdio.h>

#define  LOG_TAG    "hb-4"
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

struct NativeWorkerArgs
{
    jint id;
    jint iterations;
};

//能被缓存的方法id
static jmethodID  gOnNativeMessage = NULL;

//Java虚拟机接口指针
static JavaVM* gVm = NULL;

//包含native方法实例的全局引用
static jobject  gObj = NULL;
static pthread_mutex_t mutex;


jint JNI_OnLoad(JavaVM* vm,void* reserved)
{
    LOGI("JNI_Onload");
    //缓存Java虚拟机接口指针
    gVm = vm;
    return JNI_VERSION_1_4;
}


extern "C"
void
Java_com_example_chen_myapplication_MainActivity_nativeInit(JNIEnv *env, jobject instance) {

    LOGI("nativeInit");

    //初始互斥锁
    if (0 != pthread_mutex_init(&mutex, NULL))
    {
        // 获取异常类
        jclass exceptionClazz = env->FindClass(
                "java/lang/RuntimeException");

        // 抛出异常
         env->ThrowNew(exceptionClazz, "Unable to initialize mutex");
         goto exit;
    }

    //如果全局引用对象没有赋值
    if (NULL == gObj)
    {
        //为包含native方法的类创建一个全局引用
        gObj = env->NewGlobalRef(instance);

        if (NULL == gObj)
        {
            goto exit;
        }

    }

    //如果方法id没有缓存
    if (NULL == gOnNativeMessage)
    {
        //获取对象所属的类
        jclass clazz = env->GetObjectClass(instance);

        //获取方法标示
        gOnNativeMessage = env->GetMethodID(clazz,"onNativeMessage","(Ljava/lang/String;)V");
        if (NULL == gOnNativeMessage){
            jclass exceptionClazz = env->FindClass("java/lang/RuntimeException");
            //抛出异常
            env->ThrowNew(exceptionClazz,"Unable to find method");
        }
    }
    exit:return;
}extern "C"
JNIEXPORT void JNICALL
Java_com_example_chen_myapplication_MainActivity_nativeFree(JNIEnv *env, jobject instance) {

    LOGI("nativeFree");
    //如果全局对象引用没有设置
    if(NULL != gObj)
    {
        //删除全局引用
        env->DeleteGlobalRef(gObj);
        gObj = NULL;
    }

    //销毁互斥锁
    if (0!=pthread_mutex_destroy(&mutex))
    {
        //获取异常类
        jclass exceptionClazz = env->FindClass("java/lang/RuntimeException");

        //抛出异常
        env->ThrowNew(exceptionClazz,"Unable to destroy mutex");
    }

}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_chen_myapplication_MainActivity_nativeWorker(JNIEnv *env, jobject instance,
                                                              jint id, jint iterations) {

    LOGI("nativeWorker ");
    //锁定互斥锁
    if (0!= pthread_mutex_lock(&mutex))
    {
        // 获取异常类
        jclass exceptionClazz = env->FindClass(
                "java/lang/RuntimeException");

        // 抛出异常
        env->ThrowNew(exceptionClazz, "Unable to lock mutex");
        goto exit;
    }

    for (jint i= 0; i <iterations ; i++) {
            //准备message
        char message[26];
        sprintf(message,"Worker %d:Iteration %d",id,i);

        //c字符串转换为Java字符串
        jstring  messageString = env->NewStringUTF(message);
        //调用原生消息方法
        env->CallVoidMethod(instance,gOnNativeMessage,messageString);

        //检查是否产生异常
        if(NULL != env->ExceptionOccurred())
        {
            break;
            sleep(1);
        }

        //解锁互斥锁
        if(0!=pthread_mutex_unlock(&mutex))
        {
            //获取异常类
            jclass exceptionClazz = env->FindClass(
                    "java/lang/RuntimeException");

            // 抛出异常
            env->ThrowNew(exceptionClazz, "Unable to unlock mutex");
        }

    }
    exit:
    return;
}

static  void * nativeWorkerThread(void *args)
{

    LOGI("nativeWorkerThread");
    JNIEnv *env = NULL;

    //将当前线程附加到Java 虚拟机上
    //并且获得JNIEnv接口指针

    if (0==gVm->AttachCurrentThread(&env,NULL))
    {
        //获得原生worker线程参数
        NativeWorkerArgs* nativeWorkerArgs = (NativeWorkerArgs*)args;
        //在线程上下文中运行原生worker
        Java_com_example_chen_myapplication_MainActivity_nativeWorker(env,
                                                                      gObj,
                                                                      nativeWorkerArgs->id,
                                                                      nativeWorkerArgs->iterations);


        //释放原生worker线程参数
        delete nativeWorkerArgs;

        //从Java虚拟机中分离线程参数
        gVm->DetachCurrentThread();
    }
    return (void*) 1;
}


extern "C"
JNIEXPORT void JNICALL
Java_com_example_chen_myapplication_MainActivity_posixThread(JNIEnv *env, jobject instance,
                                                             jint threads, jint iterations) {
    LOGI("posixThreads");
    //线程句柄
    pthread_t* handles = new pthread_t[threads];
    for (jint i = 0; i <threads ; i++) {
        //原生worker线程参数
        NativeWorkerArgs* nativeWorkerArgs = new NativeWorkerArgs;
        nativeWorkerArgs->id = i;
        nativeWorkerArgs->iterations = iterations;

        //创建一个新线程
        int result = pthread_create(&handles[i],NULL,nativeWorkerThread,nativeWorkerArgs);

        if (0!=result)
        {
            // 获取异常类
            jclass exceptionClazz = env->FindClass(
                    "java/lang/RuntimeException");

            // 抛出异常
            env->ThrowNew(exceptionClazz, "Unable to create thread");
        }
    }
    LOGI("线程创建完成");
    }


