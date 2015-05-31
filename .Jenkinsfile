#!/usr/bin/env groovy

stage 'Starting CI check'
node {
}

//////////////////////

stage 'Parallel builds'

def labels = [
    "OSX-10.12",
    "Ubuntu-16.04-64bit",
    "code-coverage",
]

def builds = [:]

// Apparently, cannot use for-loop iteration, see https://issues.jenkins-ci.org/browse/JENKINS-27421
for (int i = 0; i < labels.size(); i++) {
  String label = labels.get(i);

  builds[label] = {
    node(label) {
      wrap([$class: 'hudson.plugins.ansicolor.AnsiColorBuildWrapper', colorMapName: 'xterm']) {

        deleteDir()
        git url: 'https://gerrit.named-data.net/ChronoShare'
        sh "git fetch origin ${GERRIT_REFSPEC} && git checkout FETCH_HEAD"

        withEnv(["NODE_LABELS=\"$label\""]) {
          sh "./.jenkins"
        }

        if (label == "code-coverage") {
          publishHTML(target: [
                      allowMissing: false,
                               alwaysLinkToLastBuild: false,
                               keepAll: true,
                               reportDir: 'build/coverage',
                               reportFiles: 'index.html',
                               reportName: "GCOVR Coverage Report"
                               ])
          // CoverturaPublisher is not yet supported by pipelines, see https://issues.jenkins-ci.org/browse/JENKINS-30700
          // step([$class: 'CoberturaPublisher',
          //       coberturaReportFile: 'build/coverage.xml',
          //       autoUpdateHealth: false, autoUpdateStability: false, zoomCoverageChart: true,
          //       failNoReports: true, failUnhealthy: false, failUnstable: false,
          //       maxNumberOfBuilds: 0, onlyStable: false, sourceEncoding: 'UTF-8'])
        } else {
          // don't publish test results for code coverage run
          step([$class: 'XUnitBuilder',
                testTimeMargin: '50000', thresholdMode: 1,
                tools: [[$class: 'BoostTestJunitHudsonTestType', deleteOutputFiles: true, failIfNotNew: true,
                         pattern: 'build/xunit*.xml', skipNoTestFiles: true, stopProcessingIfError: true]]])
        }
      }
    }
  }
}

parallel builds
