#include <vaultmp>

main()
{
    printf("My first PAWN vaultscript <3\n");
    SetServerName("vaultmp 0.1a server");
    SetServerRule("website", "vaultmp.com");

    switch (GetGameCode())
    {
    case FALLOUT3:
        SetServerMap("the wasteland");
    case NEWVEGAS:
        SetServerMap("mojave desert");
    case OBLIVION:
        SetServerMap("cyrodiil");
    }
}

public OnClientAuthenticate(const name[], const pwd[])
{
	printf("PAWN: client auth %s, %s\n", name, pwd);
	return true;
}

public OnPlayerConnect(player)
{

}

public OnPlayerDisconnect(player, reason)
{
	printf("PAWN: player disconnect %d, %d\n", player, reason);
}

public OnPlayerRequestGame(player)
{
    	printf("PAWN: player game %d\n", player);
    	return 0x00030D82;
}

public OnPlayerSpawn(player)
{

}

public OnPlayerDeath(player)
{

}

public OnPlayerCellChange(player, cell)
{

}