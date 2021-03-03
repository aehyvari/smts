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
WORKDIR /home/smts/
ENV NOTVISIBLE "in users profile"
RUN echo "export VISIBLE=now" >> /etc/profile

#USER smts
#CMD ["/usr/sbin/sshd", "-D", "-f", "/home/smts/.ssh/sshd_config"]
EXPOSE 22
################
FROM ubuntu:18.04 AS builder
ENV CMAKE_BUILD_TYPE Release
ENV INSTALL /home/opensmt
ENV EXTERNALREPODIR opensmt
ENV USE_READLINE OFF
ENV FLAGS -Wall
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt install -y apt-utils make cmake \
     build-essential libgmp-dev libedit-dev libsqlite3-dev bison flex libubsan0 \
     zlib1g-dev libopenmpi-dev git sudo python3-mpi4py python3
RUN cd home; git clone https://github.com/MasoudAsadzade/SMTS.git
RUN sh home/SMTS/ci/run_travis_opensmtCommands.sh

RUN sh home/SMTS/ci/run_travis_smtsCommands.sh
CMD [ "python3", "home/SMTS/server/smts.py","-o4","-l"]

RUN  sudo -s
#ADD . mpi4py
RUN  echo 'btl_base_warn_component_unused = 0' > /etc/openmpi/openmpi-mca-params.conf
RUN  exit
#RUN  cd mpi4py

#RUN cat mpi4py/host_list
CMD [ "mpirun", "--allow-run-as-root","-n","3","python3","home/SMTS/server/smts.py"]
#CMD [ "mpirun", "--allow-run-as-root","-n","3","--hostfile","mpi4py/host_list","python3","mpi4py/mpi4.py"]