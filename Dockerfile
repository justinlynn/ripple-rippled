FROM ubuntu:12.04
MAINTAINER Ripple Labs Rippled Team "infrastructure+rippled@ripplelabs.com"

# Install Chef
RUN apt-get -y update
RUN apt-get -y install curl build-essential libxml2-dev libxslt-dev git
RUN curl -L https://www.opscode.com/chef/install.sh | bash
RUN echo "gem: --no-ri --no-rdoc" > ~/.gemrc
RUN /opt/chef/embedded/bin/gem install berkshelf

# Mount chef components inside the container
ADD env/chef /srv/chef
ADD env/dev/chef-solo.json /srv/chef/chef-solo.json
ADD env/dev/chef-solo.rb  /srv/chef/chef-solo.rb

# Converge the container
RUN cd /srv/chef && /opt/chef/embedded/bin/berks install --path /srv/chef/cookbooks
RUN chef-solo -c /srv/chef/dev-solo.rb -j /srv/chef/dev-solo.json

RUN mkdir -p /opt/ripple/rippled

WORKDIR /opt/ripple/rippled
COMMAND build/rippled --net --configfile /etc/rippled/rippled.cfg
