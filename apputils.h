#pragma once


class WindowCreationMonitor {
public:
    virtual ~WindowCreationMonitor() {}
    virtual bool init(HWND hWnd, int msgId) = 0;
    virtual bool term() = 0;
};


class EventHookWindowCreationMonitor : public WindowCreationMonitor, boost::noncopyable {
public:
    EventHookWindowCreationMonitor() {}
    ~EventHookWindowCreationMonitor() { term(); }

    bool init(HWND hWnd, int msgId) {
        if (!hook) {
            hook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_CREATE, 
                0, proc, 0, 0, WINEVENT_OUTOFCONTEXT);
            if (hook) {
                this->hWnd = hWnd;
                this->msgId = msgId;
            }
        }
        return hook != 0;
    }

    bool term() {
        if (hook && UnhookWinEvent(hook)) {
            hook = 0;
        }
        return !hook;
    }

private:
    static HWINEVENTHOOK hook;
    static HWND hWnd;
    static int msgId;

    static VOID CALLBACK proc(HWINEVENTHOOK hook, DWORD event,
        HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
    {
        if (hook == EventHookWindowCreationMonitor::hook &&
            event == EVENT_OBJECT_CREATE &&
            idObject == OBJID_WINDOW)
        {
            PostMessage(hWnd, msgId, (WPARAM)hwnd, 0);
        }
    }

};

    
class HookDllWindowCreationMonitor : public WindowCreationMonitor, boost::noncopyable {
private:
    typedef bool (*initF)(HWND hWnd, int msgId);
    typedef bool (*termF)();

public:
    HookDllWindowCreationMonitor() : hDll(0), dllInitFunc(0), dllTermFunc(0)
    {
    }

    ~HookDllWindowCreationMonitor() {
        term();
    }

    bool init(HWND hWnd, int msgId) {
        if (!hDll) {
            hDll = LoadLibrary(_T("dphook.dll"));
            if (!hDll)
                return false;
        }

        dllInitFunc = initF(GetProcAddress(hDll, "init"));
        dllTermFunc = termF(GetProcAddress(hDll, "term"));
        if (!dllInitFunc || !dllTermFunc) {
            term();
            return false;
        }

        return dllInitFunc(hWnd, msgId);
    }

    bool term()
    {
        if (!hDll)
            return true;

        if (dllTermFunc)
            dllTermFunc();

        bool ok = !!FreeLibrary(hDll);
        if (ok)
            hDll = 0;

        return ok;
    }

private:
    HMODULE hDll;
    initF dllInitFunc;
    termF dllTermFunc;
};


/*// Hook DLL manager (singleton).
// Handles dynamic loading/unloading on request.
//
class HookDll : boost::noncopyable {
private:
    typedef bool (*initF)(HWND hWnd, int msgId);
    typedef bool (*termF)();

public:
    static HookDll& obj() {
        static HookDll hookDll;
        return hookDll;
    }

    ~HookDll() {
        term();
    }

    bool init(HWND hWnd, int msgId);
    bool term();

private:
    HookDll() : hDll(0), dllInitFunc(0), dllTermFunc(0)
    {
    }

    HookDll(const HookDll&);
    HookDll& operator=(const HookDll&);

    HMODULE hDll;
    initF dllInitFunc;
    termF dllTermFunc;

    /*bool isLoaded() {
        return hDll != 0;
    }*-/

};*/


class Options;

// Manager of new windows for autopin checking.
//
class PendingWindows {
public:
    void add(HWND hWnd);
    void check(HWND hWnd, const Options& opt);

protected:
    struct Entry {
        HWND  hWnd;
        DWORD time;
        Entry(HWND h = 0, DWORD t = 0) : hWnd(h), time(t) {}
    };
    std::vector<Entry> entries;

    bool timeToChkWnd(DWORD t, const Options& opt);
    bool checkWnd(HWND hTarget, const Options& opt);

};
