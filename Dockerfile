FROM ubuntu:18.04 AS smts_base
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt install -y openssh-server iproute2 openmpi-bin openmpi-common iputils-ping \
    && mkdir /var/run/sshd \
    && sed 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' -i /etc/pam.d/sshd \
    && setcap CAP_NET_BIND_SERVICE=+eip /usr/sbin/sshd \
    && useradd -ms /bin/bash smts \
    && chown -R smts /etc/ssh/ \
    && su - smts -c \
        'ssh-keygen -q -t rsa -f ~/.ssh/id_rsa -N "" \
        && cp ~/.ssh/id_rsa.pub ~/.ssh/authorized_keys \
        && cp /etc/ssh/sshd_config ~/.ssh/sshd_config \
        && sed -i "s/UsePrivilegeSeparation yes/UsePrivilegeSeparation no/g" ~/.ssh/sshd_config \
        && printf "Host *\n\tStrictHostKeyChecking no\n" >> ~/.ssh/config'
#WORKDIR /SMTS/
ENV NOTVISIBLE "in users profile"
RUN echo "export VISIBLE=now" >> /etc/profile

#USER smts
#CMD ["/usr/sbin/sshd", "-D", "-f", "/home/smts/.ssh/sshd_config"]
EXPOSE 22
################
FROM ubuntu:18.04 AS builder
ENV CMAKE_BUILD_TYPE Release
ENV INSTALL SMTS/opensmt
ENV USE_READLINE OFF
ENV FLAGS -Wall
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt install -y apt-utils make cmake \
     build-essential libgmp-dev libedit-dev libsqlite3-dev bison flex libubsan0 \
     zlib1g-dev libopenmpi-dev git
RUN git clone https://github.com/MasoudAsadzade/SMTS.git
RUN cd SMTS && sh ./ci/run_travis_opensmtCommands.sh
RUN cd SMTS && sh ./ci/run_travis_smtsCommands.sh

FROM smts_base AS smts_liaison
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt install -y awscli python3 mpi python python-pip
COPY --from=builder SMTS/ /SMTS


#COPY --from=builder /SMTS /SMTS
ADD make_combined_hostfile.py supervised-scripts/make_combined_hostfile.py
RUN chmod 755 supervised-scripts/make_combined_hostfile.py
ADD mpi-run.sh supervised-scripts/mpi-run.sh
USER smts
CMD ["/usr/sbin/sshd", "-D", "-f", ".ssh/sshd_config"]
#CMD sh supervised-scripts/mpi-run.sh
#WORKDIR /SMTS/
RUN sleep 9000000
#CMD [ "python3", "SMTS/server/smts.py","-o4","-l"]
