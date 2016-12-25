function prefix()
{
  var tmp = document.getElementById("prefixcontent").innerHTML;
  var data = tmp.split("END");
  var odd = "odd";

  for (var i in data) {
    // Parse the JSON data.
    var obj = jQuery.parseJSON(data[i]);

    if (i % 2 == 0) {
      odd = "";
    }
    else {
      odd = "odd";
    }

    // Create the HTML for each prefix.
    var output = '<tr class="' + odd + '">\n';

    output +=
      '<td rowspan="' + obj.prefixes.length + '">' + obj.router + '</td>';


    for (var i in obj.prefixes) {
      output += '<td class="' + odd + '">' + obj.prefixes[i].timestamp + '</td>';
      output += '<td class="' + odd + '">' + obj.prefixes[i].prefix + '</td>';

      if (obj.prefixes[i].status == "notintopology") {
        output += '<td id="' + obj.prefixes[i].status + '">NPT</td>';
      }
      else {
        output += '<td id="' + obj.prefixes[i].status + '">' +
                  obj.prefixes[i].status + '</td>';
      }

      output += '</tr>';
    }

    // Append the data to the prefix table.
    $('.one > tbody:last').append(output);
  }
}

function link()
{
  var tmp = document.getElementById("linkcontent").innerHTML;
  var data = tmp.split("END");
  var odd = "odd";

  for (var i in data) {
    // Parse the JSON data.
    var obj = jQuery.parseJSON(data[i]);

    if (i % 2 == 0) {
      odd = "";
    }
    else {
      odd = "odd";
    }

    // Create the HTML for each prefix.
    var output = '<tr class="' + odd + '">\n';

    output += '<td rowspan="' + obj.links.length + '">' + obj.router + '</td>';
    output +=
      '<td rowspan="' + obj.links.length + '">' + obj.timestamp + '</td>';

    for (var i in obj.links) {
      output +=
        '<td id="' + obj.links[i].status + '">' + obj.links[i].link + '</td>';

      if (obj.links[i].status == "notintopology") {
        output += '<td id="' + obj.links[i].status + '">NPT</td>';
      }
      else {
        output += '<td id="' + obj.links[i].status + '">' +
                  obj.links[i].status + '</td>';
      }

      output += '</tr>';
    }

    // Append the data to the prefix table.
    $('.two > tbody:last').append(output);
  }
}
