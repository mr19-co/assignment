FROM ubuntu:jammy

ARG USER=default_user
ARG USER_UID=1000
ARG USER_GID=$USER_UID

# Create the user
RUN groupadd --gid $USER_GID $USER \
    && useradd --uid $USER_UID --gid $USER_GID -m $USER \
    && usermod --shell /bin/bash  $USER

RUN apt-get update && apt-get install -y git wget gpg software-properties-common vim

RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor -o /usr/share/keyrings/llvm.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/llvm.gpg] http://apt.llvm.org/jammy/ llvm-toolchain-jammy-19 main" >> /etc/apt/sources.list \
    && apt-get update \
    && apt-get install -y llvm-19 llvm-19-dev llvm-19-tools clang-19

# Add sudo support
RUN apt-get update \
    && apt-get install -y sudo \
    && usermod -aG sudo $USER \
    && echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers
USER $USER

WORKDIR /home/$USER
