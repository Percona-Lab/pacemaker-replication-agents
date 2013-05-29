# Copyright (c) 2009 Red Hat, Inc.
#
# Authors: Andrew Beekhof
#	   Steven Dake (sdake@redhat.com)
#
# This software licensed under BSD license, the text of which follows:
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# - Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
# - Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# - Neither the name of the MontaVista Software, Inc. nor the names of its
#   contributors may be used to endorse or promote products derived from this
#   software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

SPEC			= $(PACKAGE_NAME).spec

TARFILE			= $(PACKAGE_NAME)-$(VERSION).tar.gz

EXTRA_DIST		= autogen.sh conf/booth.conf.example $(bootharbitrator_SCRIPTS) $(boothsite_SCRIPTS) 

AUTOMAKE_OPTIONS	= foreign

MAINTAINERCLEANFILES	= Makefile.in aclocal.m4 configure depcomp \
			  config.guess config.sub missing install-sh \
			  autoheader automake autoconf test_lense.sh

dist_doc_DATA		= README COPYING

boothconfdir		= ${BOOTHSYSCONFDIR}

boothconf_DATA		= conf/booth.conf.example

boothsitedir		= /usr/lib/ocf/resource.d/pacemaker

boothsite_SCRIPTS	= script/ocf/booth-site

bootharbitratordir	= ${INITDDIR}

bootharbitrator_SCRIPTS	= script/lsb/booth-arbitrator 

TESTS			= test/runtests.py

SUBDIRS			= src

coverity:
	cov-build --dir=cov make
	cov-analyze --dir cov --concurrency --wait-for-license
	cov-format-errors --dir cov

install-exec-local:
	$(INSTALL) -d $(DESTDIR)/${boothconfdir}
	$(INSTALL) -d $(DESTDIR)/${bootharbitratordir}
	$(INSTALL) -d $(DESTDIR)/${boothsitedir}
	$(INSTALL) -d $(DESTDIR)/${SOCKETDIR}

install-exec-hook:
	ln -sf ${sbindir}/boothd $(DESTDIR)/${sbindir}/booth

uninstall-local:
	rmdir $(DESTDIR)/${boothconfdir} || :;
	rmdir $(DESTDIR)/${bootharbitratordir} || :;
	rmdir $(DESTDIR)/${boothsitedir} || :;
	rmdir $(DESTDIR)/${SOCKETDIR} || :;

test: check

lint:
	for dir in src; do make -C $$dir lint; done

dist-clean-local:
	rm -f autoconf automake autoheader

dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version


## make rpm/srpm section.

$(SPEC): $(SPEC).in
	rm -f $@-t $@
	date="$(shell LC_ALL=C date "+%a %b %d %Y")" && \
	if [ -f .tarball-version ]; then \
		gitver="$(shell cat .tarball-version)" && \
		rpmver=$$gitver && \
		alphatag="" && \
		dirty="" && \
		numcomm="0"; \
	else \
		gitver="$(shell git describe --tags --abbrev=4 --match='v*' HEAD 2>/dev/null)" && \
		rpmver=`echo $$gitver | sed -e "s/^v//" -e "s/-.*//g"` && \
		alphatag=`echo $$gitver | sed -e "s/.*-//" -e "s/^g//"` && \
		vtag=`echo $$gitver | sed -e "s/-.*//g"` && \
		numcomm=`git rev-list $$vtag..HEAD | wc -l` && \
		git update-index --refresh > /dev/null 2>&1 || true && \
		dirty=`git diff-index --name-only HEAD 2>/dev/null`; \
	fi && \
	if [ -n "$$dirty" ]; then dirty="dirty"; else dirty=""; fi && \
	if [ "$$numcomm" = "0" ]; then \
		sed \
			-e "s#@version@#$$rpmver#g" \
			-e "s#%glo.*alpha.*##g" \
			-e "s#%glo.*numcomm.*##g" \
			-e "s#@dirty@#$$dirty#g" \
			-e "s#@date@#$$date#g" \
		$< > $@-t; \
	else \
		sed \
			-e "s#@version@#$$rpmver#g" \
			-e "s#@alphatag@#$$alphatag#g" \
			-e "s#@numcomm@#$$numcomm#g" \
			-e "s#@dirty@#$$dirty#g" \
			-e "s#@date@#$$date#g" \
		$< > $@-t; \
	fi; \
	if [ -z "$$dirty" ]; then sed -i -e "s#%glo.*dirty.*##g" $@-t; fi
	chmod a-w $@-t
	mv $@-t $@

$(TARFILE):
	$(MAKE) dist

RPMBUILDOPTS	= --define "_sourcedir $(abs_builddir)" \
		  --define "_specdir $(abs_builddir)" \
		  --define "_builddir $(abs_builddir)" \
		  --define "_srcrpmdir $(abs_builddir)" \
		  --define "_rpmdir $(abs_builddir)"

srpm: clean
	autoreconf -if
	$(MAKE) $(SPEC) $(TARFILE)
	rpmbuild $(RPMBUILDOPTS) --nodeps -bs $(SPEC)

rpm: clean
	autoreconf -if
	$(MAKE) $(SPEC) $(TARFILE)
	rpmbuild $(RPMBUILDOPTS) -ba $(SPEC)
