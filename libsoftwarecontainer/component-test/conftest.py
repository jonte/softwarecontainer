
# Copyright (C) 2016 Pelagicore AB
#
# Permission to use, copy, modify, and/or distribute this software for
# any purpose with or without fee is hereby granted, provided that the
# above copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
# BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
# OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
# WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
# ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
# SOFTWARE.
#
# For further information see LICENSE


import pytest

"""
    Copyright (C) 2014 Pelagicore AB
    All rights reserved.
"""

def pytest_addoption(parser):
    parser.addoption("--softwarecontainer-binary", action="store", default=None,
        help="Path to softwarecontainer binary")
    parser.addoption("--container-path", action="store", default=None,
        help="Path to container")

@pytest.fixture(scope="module")
def softwarecontainer_binary(request):
    return request.config.getoption("--softwarecontainer-binary")

@pytest.fixture(scope="module")
def container_path(request):
    option = request.config.getoption("--container-path")
    if option.endswith("/"):
      return option
    else:
      return option + "/"

@pytest.fixture
def teardown_fixture(request):
    """ Teardown run after each test run (both successful and failed).
        SoftwareContainer will be shutdown if there is an assertion failure in the
        running test.

        Pass this fixture to any test function that should shutdown SoftwareContainer
        upon assertion failure.
    """
    helpers = list()
    helper_attribute = getattr(request.module, "helper")
    # The helper attribute of the requesting module should be either an
    # instance of a test-helper or a list of instances
    if type(helper_attribute) is list:
        helpers = helper_attribute
    else:
        helpers.append(helper_attribute)

    def teardown():
        try:
            if request.node.rep_setup.failed:
                print "Setup failed, shutting down SoftwareContainer"
                for helper in helpers:
                    helper.teardown()
        except AttributeError as e:
            pass

        try:
            if request.node.rep_call.failed:
                print "A test failed, shutting down SoftwareContainer"
                for helper in helpers:
                    helper.teardown()
        except AttributeError as e:
            pass

    request.addfinalizer(teardown)

@pytest.mark.tryfirst
def pytest_runtest_makereport(item, call, __multicall__):
    """ This is a helper function that may be used from within other fixtures
        to determine whether setup, a test, or teardown has failed.

        The report attribute is accessed through the requesting context object
        in a fixture, e.g.:
        \code if request.node.rep_setup.failed: do_something()
        or
        \code if request.node.rep_call.failed: do_something()

        Example from: https://pytest.org/latest/example/simple.html
    """
    # Execute all other hooks to obtain the report object
    rep = __multicall__.execute()

    # Set a report attribute for each phase of a call, which can be "setup", "call", "teardown"
    setattr(item, "rep_" + rep.when, rep)
    return rep
