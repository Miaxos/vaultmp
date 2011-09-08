#include "Script.h"

vector<Script*> Script::scripts;

Script::Script(char* path)
{
    FILE* file = fopen(path, "rb");

    if (file == NULL)
        throw VaultException("Script not found: %s", path);

    fclose(file);

    if (strstr(path, ".dll") || strstr(path, ".so"))
    {
        void* handle = NULL;
#ifdef __WIN32__
        handle = LoadLibrary(path);
#else
        handle = dlopen(path, RTLD_LAZY);
#endif
        if (handle == NULL)
            throw VaultException("Was not able to load C++ script: %s", path);

        this->handle = handle;
        this->cpp_script = true;
        scripts.push_back(this);

        GetScriptCallback(fexec, "exec", exec);
        GetScriptCallback(fOnClientAuthenticate, "OnClientAuthenticate", OnClientAuthenticate);
        GetScriptCallback(fOnPlayerDisconnect, "OnPlayerDisconnect", OnPlayerDisconnect);
        GetScriptCallback(fOnPlayerRequestGame, "OnPlayerRequestGame", OnPlayerRequestGame);
        GetScriptCallback(fOnSpawn, "OnSpawn", OnSpawn);
        GetScriptCallback(fOnCellChange, "OnCellChange", OnCellChange);
        GetScriptCallback(fOnActorDeath, "OnActorDeath", OnActorDeath);
        GetScriptCallback(fOnActorValueChange, "OnActorValueChange", OnActorValueChange);
        GetScriptCallback(fOnActorBaseValueChange, "OnActorBaseValueChange", OnActorBaseValueChange);
        GetScriptCallback(fOnActorStateChange, "OnActorStateChange", OnActorStateChange);

        SetScriptFunction("timestamp", &Utils::timestamp);
        SetScriptFunction("CreateTimer", &Script::CreateTimer);
        SetScriptFunction("KillTimer", &Script::KillTimer);

        SetScriptFunction("SetServerName", &Dedicated::SetServerName);
        SetScriptFunction("SetServerMap", &Dedicated::SetServerMap);
        SetScriptFunction("SetServerRule", &Dedicated::SetServerRule);
        SetScriptFunction("GetGameCode", &Dedicated::GetGameCode);

        SetScriptFunction("ValueToString", &API::RetrieveValue_Reverse);
        SetScriptFunction("AxisToString", &API::RetrieveAxis_Reverse);
        SetScriptFunction("AnimToString", &API::RetrieveAnim_Reverse);

        SetScriptFunction("GetPos", &Script::GetPos);
        SetScriptFunction("GetAngle", &Script::GetAngle);
        SetScriptFunction("GetCell", &Script::GetCell);
        SetScriptFunction("GetActorValue", &Script::GetActorValue);
        SetScriptFunction("GetActorBaseValue", &Script:: GetActorBaseValue);

        exec();
    }
    else if (strstr(path, ".amx"))
    {
        AMX* vaultscript = new AMX();

        this->handle = (void*) vaultscript;
        this->cpp_script = false;
        scripts.push_back(this);

        cell ret = 0;
        int err = 0;

        err = PAWN::LoadProgram(vaultscript, path, NULL);
        if (err != AMX_ERR_NONE)
            throw VaultException("PAWN script %s error (%d): \"%s\"", path, err, aux_StrError(err));

        PAWN::CoreInit(vaultscript);
        PAWN::ConsoleInit(vaultscript);
        PAWN::FloatInit(vaultscript);
        PAWN::StringInit(vaultscript);
        PAWN::FileInit(vaultscript);
        PAWN::TimeInit(vaultscript);

        err = PAWN::RegisterVaultmpFunctions(vaultscript);
        if (err != AMX_ERR_NONE)
            throw VaultException("PAWN script %s error (%d): \"%s\"", path, err, aux_StrError(err));

        err = PAWN::Exec(vaultscript, &ret, AMX_EXEC_MAIN);
        if (err != AMX_ERR_NONE)
            throw VaultException("PAWN script %s error (%d): \"%s\"", path, err, aux_StrError(err));
    }
    else
        throw VaultException("Script type not recognized: %s", path);
}

Script::~Script()
{
    if (this->cpp_script)
    {
#ifdef __WIN32__
        FreeLibrary((HINSTANCE) this->handle);
#else
        dlclose(this->handle);
#endif
    }
    else
    {
        AMX* vaultscript = (AMX*) this->handle;
        PAWN::FreeProgram(vaultscript);
        delete vaultscript;
    }
}

void Script::LoadScripts(char* scripts, char* base)
{
    char* token = strtok(scripts, ",");

    try
    {
        while (token != NULL)
        {
#ifdef __WIN32__
            Script* script = new Script(token);
#else
            char path[MAX_PATH];
            snprintf(path, sizeof(path), "%s/%s", base, token);
            Script* script = new Script(path);
#endif
            token = strtok(NULL, ",");
        }
    }
    catch (...)
    {
        UnloadScripts();
        throw;
    }
}

void Script::UnloadScripts()
{
    vector<Script*>::iterator it;

    for (it = scripts.begin(); it != scripts.end(); ++it)
        delete *it;

    Timer::TerminateAll();
    scripts.clear();
}

NetworkID Script::CreateTimer(TimerFunc timer, unsigned int interval)
{
    Timer* t = new Timer(timer, interval);
    return t->GetNetworkID();
}

NetworkID Script::CreateTimerPAWN(TimerPAWN timer, AMX* amx, unsigned int interval)
{
    Timer* t = new Timer(timer, amx, interval);
    return t->GetNetworkID();
}

void Script::KillTimer(NetworkID id)
{
    Timer::Terminate(id);
}

bool Script::Authenticate(string name, string pwd)
{
    vector<Script*>::iterator it;
    bool result;

    for (it = scripts.begin(); it != scripts.end(); ++it)
    {
        if ((*it)->cpp_script)
            result = (*it)->OnClientAuthenticate(name, pwd);
        else
            result = (bool) PAWN::Call((AMX*) (*it)->handle, "OnClientAuthenticate", "ss", 0, pwd.c_str(), name.c_str());
    }

    return result;
}

unsigned int Script::RequestGame(NetworkID id)
{
    vector<Script*>::iterator it;
    unsigned int result;

    for (it = scripts.begin(); it != scripts.end(); ++it)
    {
        if ((*it)->cpp_script)
            result = (*it)->OnPlayerRequestGame(id);
        else
            result = (unsigned int) PAWN::Call((AMX*) (*it)->handle, "OnPlayerRequestGame", "f", 0, id);
    }

    return result;
}

void Script::Disconnect(NetworkID id, unsigned char reason)
{
    vector<Script*>::iterator it;

    for (it = scripts.begin(); it != scripts.end(); ++it)
    {
        if ((*it)->cpp_script)
            (*it)->OnPlayerDisconnect(id, reason);
        else
            PAWN::Call((AMX*) (*it)->handle, "OnPlayerDisconnect", "if", 0, (unsigned int) reason, id);
    }
}

void Script::CellChange(NetworkID id, unsigned int cell)
{
    vector<Script*>::iterator it;

    for (it = scripts.begin(); it != scripts.end(); ++it)
    {
        if ((*it)->cpp_script)
            (*it)->OnCellChange(id, cell);
        else
            PAWN::Call((AMX*) (*it)->handle, "OnCellChange", "if", 0, cell, id);
    }
}

void Script::ValueChange(NetworkID id, unsigned char index, bool base, double value)
{
    vector<Script*>::iterator it;

    for (it = scripts.begin(); it != scripts.end(); ++it)
    {
        if ((*it)->cpp_script)
        {
            if (base)
                (*it)->OnActorBaseValueChange(id, index, value);
            else
                (*it)->OnActorValueChange(id, index, value);
        }
        else
        {
            if (base)
                PAWN::Call((AMX*) (*it)->handle, "OnActorBaseValueChange", "fif", 0, value, (unsigned int) index, id);
            else
                PAWN::Call((AMX*) (*it)->handle, "OnActorValueChange", "fif", 0, value, (unsigned int) index, id);
        }
    }
}

void Script::StateChange(NetworkID id, unsigned char index, bool alerted)
{
    vector<Script*>::iterator it;

    for (it = scripts.begin(); it != scripts.end(); ++it)
    {
        if ((*it)->cpp_script)
            (*it)->OnActorStateChange(id, index, alerted);
        else
            PAWN::Call((AMX*) (*it)->handle, "OnActorStateChange", "iif", 0, (unsigned int) alerted, (unsigned int) index, id);
    }
}

void Script::GetPos(NetworkID id, double& X, double& Y, double& Z)
{
    X = 0.00;
    Y = 0.00;
    Z = 0.00;

    Object* object = (Object*) GameFactory::GetObject(ALL_OBJECTS, id);

    if (object)
    {
        X = object->GetPos(Axis_X);
        Y = object->GetPos(Axis_Y);
        Z = object->GetPos(Axis_Z);
        GameFactory::LeaveReference(object);
    }
}

void Script::GetAngle(NetworkID id, double& X, double& Y, double& Z)
{
    X = 0.00;
    Y = 0.00;
    Z = 0.00;

    Object* object = (Object*) GameFactory::GetObject(ALL_OBJECTS, id);

    if (object)
    {
        X = object->GetAngle(Axis_X);
        Y = object->GetAngle(Axis_Y);
        Z = object->GetAngle(Axis_Z);
        GameFactory::LeaveReference(object);
    }
}

unsigned int Script::GetCell(NetworkID id)
{
    unsigned int value = 0;

    Object* object = (Object*) GameFactory::GetObject(ALL_OBJECTS, id);

    if (object)
    {
        value = object->GetGameCell();
        GameFactory::LeaveReference(object);
    }

    return value;
}

double Script::GetActorValue(NetworkID id, unsigned char index)
{
    double value = 0.00;
    Actor* actor = (Actor*) GameFactory::GetObject(ALL_ACTORS, id);

    if (actor)
    {
        try
        {
            value = actor->GetActorValue(index);
        }
        catch (...)
        {
            GameFactory::LeaveReference(actor);
            throw;
        }

        GameFactory::LeaveReference(actor);
    }

    return value;
}

double Script::GetActorBaseValue(NetworkID id, unsigned char index)
{
    double value = 0.00;
    Actor* actor = (Actor*) GameFactory::GetObject(ALL_ACTORS, id);

    if (actor)
    {
        try
        {
            value = actor->GetActorBaseValue(index);
        }
        catch (...)
        {
            GameFactory::LeaveReference(actor);
            throw;
        }

        GameFactory::LeaveReference(actor);
    }

    return value;
}
