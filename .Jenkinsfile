//////////////////////

stage 'Checkout from git'
node {
    deleteDir()
    git url: 'http://gerrit.named-data.net/ndn-tools'
    sh "git fetch origin ${GERRIT_REFSPEC} && git checkout FETCH_HEAD"
    stash name: 'code', includes: '**/*, .git/**/*', useDefaultExcludes: false
}

//////////////////////

stage 'Parallel builds'

def labels = [
    "OSX-10.12",
    "Ubuntu-16.04-64bit",
    "code-coverage"
]

def builds = [:]

// Apparently, cannot use for-loop iteration, see https://issues.jenkins-ci.org/browse/JENKINS-27421
for (int i = 0; i < labels.size(); i++) {
    String label = labels.get(i);

    builds[label] = {
        node(label) {
            wrap([$class: 'hudson.plugins.ansicolor.AnsiColorBuildWrapper', colorMapName: 'xterm']) {
                deleteDir()
                unstash "code"

                sh "XUNIT=yes ./.jenkins"

                if (label == "code-coverage") {
                  step([$class: 'CoberturaPublisher', autoUpdateHealth: false, autoUpdateStability: false, coberturaReportFile: 'build/coverage.xml', failNoReports: true, failUnhealthy: false, failUnstable: false, maxNumberOfBuilds: 0, onlyStable: false, sourceEncoding: 'ASCII', zoomCoverageChart: false])
                } else {
                  // don't publish test results for code coverage run
                  step([$class: 'XUnitBuilder', testTimeMargin: '50000', thresholdMode: 1, tools: [[$class: 'BoostTestJunitHudsonTestType', deleteOutputFiles: true, failIfNotNew: true, pattern: 'build/xunit*.xml', skipNoTestFiles: true, stopProcessingIfError: true]]])
                }
            }
        }
    }
}

parallel builds
