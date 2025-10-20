#include <olog.h>
#include <stdio.h>
#include <stdlib.h>

int
main()
{
	olog_init_unmanaged(stderr);
	olog_set_cntxt(info);
	olog_msg("Hi me umer");
	olog_msg("Hi %s", "umer");
	olog_msg("bitch %d", 76);
	olog_msg_verbose("verbose test");
	olog_close();
	return EXIT_SUCCESS;
}
