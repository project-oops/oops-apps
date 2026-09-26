/* An install puts opus headers under `opus/`; the source tree has them flat. Consumers that
   write `<opus/opus_multistream.h>` land here, and this reaches the real one alongside it. */
#include <opus_multistream.h>
