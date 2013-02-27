
  hostip = "127.0.0.1";
        
  var AsyncGetClosure = function AsyncGetClosure() {
                      Closure.call(this);
              };

              AsyncGetClosure.prototype.upcall = function(kind, upcallInfo, tmp) {
                      if (kind == Closure.UPCALL_FINAL) {
                              // Do nothing.
                      } else if (kind == Closure.UPCALL_CONTENT) {
                              var content = upcallInfo.contentObject;
      var nameStr = content.name.getName().split("/").slice(5,6);
      
      if (nameStr == "prefix") {
        document.getElementById('prefixcontent').innerHTML = DataUtils.toString(content.content);
        prefix();
      } else if (nameStr == "link") {
        document.getElementById('linkcontent').innerHTML = DataUtils.toString(content.content);
        link();
      } else {
        var data = DataUtils.toString(content.content);
        var obj = jQuery.parseJSON(data);
        document.getElementById("lastupdated").innerHTML = obj.lastupdated;
        document.getElementById("lastlog").innerHTML = obj.lastlog;
        document.getElementById("lasttimestamp").innerHTML = obj.lasttimestamp;
      }
                      } else if (kind == Closure.UPCALL_INTEREST_TIMED_OUT) {
                              console.log("Closure.upcall called with interest time out.");
                      }
                      return Closure.RESULT_OK;
              };

              function getStatus(name) {
    // Template interest to get the latest content.
    var interest = new Interest("/tmp/");
                      interest.childSelector = 1;
                      interest.interestLifetime = 4000;

                      ndn.expressInterest(new Name("/ndn/memphis.edu/netlab/status/" + name), new AsyncGetClosure(), interest);
  }

  // Calls to get the content data.
  function begin() {
    getStatus("metadata");
    getStatus("prefix");
    getStatus("link");
  }

  var ndn;
  $(document).ready(function() {
    $("#all").fadeIn(500);
    var res = detect();

    if (!res) {
      $("#base").fadeOut(50);
      $("#nosupport").fadeIn(500);
    }
    else {
      openHandle = function() { console.log("NDN established"); };
      ndn = new NDN({host:hostip, onopen:openHandle});
      ndn.transport.connectWebSocket(ndn);
    }
  });

