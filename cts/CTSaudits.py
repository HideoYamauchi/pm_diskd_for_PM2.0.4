#!/usr/bin/env python

'''CTS: Cluster Testing System: Audit module
 '''

__copyright__='''
Copyright (C) 2000, 2001 Alan Robertson <alanr@unix.sh>
Licensed under the GNU GPL.
'''

#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

import time
import CTS


class ClusterAudit:

    def __init__(self, cm):
        self.CM = cm

    def __call__(self):
         raise ValueError("Abstract Class member (__call__)")

    def name(self):
         raise ValueError("Abstract Class member (name)")

class ResourceAudit(ClusterAudit):

    def name(self):
        return "ResourceAudit"

    def _doauditRsc(self, resource):
        ResourceNodes = []
        for node in self.CM.Env["nodes"]:
            if self.CM.ShouldBeStatus[node] == self.CM["up"]:
                if resource.IsRunningOn(node):
                    ResourceNodes.append(node)
	return ResourceNodes

    def _doaudit(self):
        '''Check to see if all resources are running in exactly one place
        in the cluster.
	We also verify that the members of a resource group are all
	running on the same node in the cluster,
	and we monitor that they are all running "properly".
        '''
        Fatal = 0
        result = []

        Groups = self.CM.ResourceGroups()
        for group in Groups:
            GrpServedBy = None

            for resource in group:

		#
		# _doauditRsc returns the set of nodes serving
		# the given resource.  This is normally a single node.
		#

                ResourceNodes = self._doauditRsc(resource)


		#	Is the resource served without quorum present?

		if not self.CM.HasQuorum() and len(ResourceNodes) != 0:

                    result.append("Resource " + repr(resource)
                    +	" active without Quorum: "
                    +	repr(ResourceNodes))

		#	Is the resource served at all?

                elif len(ResourceNodes) == 0 and self.CM.HasQuorum():
                    result.append("Resource " + repr(resource)
                    +	" not served anywhere.")

		# Is the resource served too many times?

                elif len(ResourceNodes) > 1:
                    result.append("Resource " + repr(resource)
                    +	" served too many times: "
                    +	repr(ResourceNodes))
                    self.CM.log("Resource " + repr(resource)
                    +	" served too many times: "
                    +	repr(ResourceNodes))
                    Fatal = 1
                elif GrpServedBy == None:
                    GrpServedBy = ResourceNodes

		# Are all the members of the Rsc Grp served by the same node?

                elif GrpServedBy != ResourceNodes:
                    result.append("Resource group" + repr(resource)
                    +	" served by different nodes: "
                    +	repr(ResourceNodes)+" vs "+repr(GrpServedBy))
                    self.CM.log("Resource " + repr(resource)
                    +	" served too many times: "
                    +	repr(ResourceNodes)+" vs "+repr(GrpServedBy))
                    Fatal = 1


		# Is the resource actually working?

                if not Fatal and len(ResourceNodes) == 1:
                    if not resource.IsWorkingCorrectly(ResourceNodes[0]):
                      result.append("Resource " + repr(resource)
                      +	" not operating properly.")

        if (Fatal):
             result.insert(0, "FATAL")  # Kludgy.

        return result


    def __call__(self):
        #
        # Audit the resources.  Since heartbeat doesn't really
        # know when resource acquisition is complete, we will
        # poll until things get stable.
        #
        # Having a resource duplicately implemented is a Fatal Error
        # with no tolerance granted.
        #
        audresult =  self._doaudit()
        #
        # Probably the constant below should be a CM parameter.
        # Then it could be 0 for FailSafe.
        # Of course, it really depends on what resources
        # you have in the test suite, and how long it takes
        # for them to settle.
        #
        audcount=60;

        while(audcount > 0):
             audresult =  self._doaudit()
             if (len(audresult) <= 0 or audresult[0] == "FATAL"):
                 audcount=0
             else:
                 audcount = audcount - 1
             if (audcount > 0):
                 time.sleep(1)
        if (len(audresult) > 0) :
            self.CM.log("Fatal Audit error: " + repr(audresult))

        return (len(audresult) == 0)

AllAuditClasses = [ ResourceAudit ]

def AuditList(cm):
    result = []
    for auditclass in AllAuditClasses:
        result.append(auditclass(cm))
    return result
