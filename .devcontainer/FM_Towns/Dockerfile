FROM debian:bookworm-slim

# Toolchain for the bare-metal FM Towns build:
#   clang  -- C compiler with -target i386-elf
#   lld    -- linker (provides ld.lld)
#   llvm   -- provides llvm-objcopy
#   nasm   -- assembler for the IPL and startup .asm files
#
# Everything else is stock build-essential (make, dd, etc.).
RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y --no-install-recommends \
        build-essential \
        clang \
        lld \
        llvm \
        nasm \
        ca-certificates \
        git && \
    rm -rf /var/lib/apt/lists/* && \
    git config --global --add safe.directory "*" && \
    SNIPPET="export PROMPT_COMMAND='history -a' && export HISTFILE=/commandhistory/.bash_history" && \
    echo "$SNIPPET" >> "/root/.bashrc"
