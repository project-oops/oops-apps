/* An install puts opus headers under `opus/`; the source tree has them flat. Consumers that
   write `<opus/opus_custom.h>` land here, and this reaches the real one alongside it. */
#include <opus_custom.h>
