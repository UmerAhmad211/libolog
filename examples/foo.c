#include <olog.h>
#include <stdlib.h>

int
main()
{
	olog_init(NULL);
	olog_set_context(info);
	olog_msg("Hi me umer");
	olog_msg("Hi %s", "umer");
	olog_msg("dog %d", 76);
	olog_msg_verbose("verbose test");
	olog_close();
	return EXIT_SUCCESS;
}
