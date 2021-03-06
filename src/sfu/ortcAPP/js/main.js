'use strict';

var isChannelReady = true;
var isInitiator = false;
var isStarted = false;
var localStream;
var pc;
var remoteStream;
var turnReady;

var room = 'foo'; /*think as a group  peerName@room */
var  remotePeerID;
var  peerID;
var  remotePeerName;
var  peerName;


var pcConfig = {
  'iceServers': [{
    'urls': 'stun:stun.l.google.com:19302'
  }]
};

// Set up audio and video regardless of what devices are present.
/*var sdpConstraints = {
  offerToReceiveAudio: true,
  offerToReceiveVideo: true
};*/

/////////////////////////////////////////////


// Could prompt for room name:
// room = prompt('Enter room name:');

var socket = io.connect();


socket.on('created', function(room) {
  console.log('Created room ' + room);
  isInitiator = true;

   socket.emit('getRouterRtpCapabilities',  function (data) { 
     console.log(data); // data will be 'tobi says woot'
     console.log("arvind " + data);
     loadDevice(data);

    });


});

socket.on('full', function(room) {
  console.log('Room ' + room + ' is full');
});

socket.on('join', function (room){
  console.log('Another peer made a request to join room ' + room);
  console.log('This peer is the initiator of room ' + room + '!');
  isChannelReady = true;
});


socket.on('joined', function(room, id) {
 console.log('joined: ' + room + ' with peerID: ' + id);
 //log('joined: ' + room + ' with peerID: ' + id);
  isChannelReady = true;
  peerID = id;



 socket.emit('getRouterRtpCapabilities',  function (data) { 
     console.log(data); // data will be 'tobi says woot'
     console.log("arvind " + data);
      loadDevice(data);

    });



  // if (isInitiator) {

  //   // when working with web enable bellow line
  //   // doCall();
  //   // disable  send message 
  //    sendMessage ({
  //     from: peerID,
  //     to: remotePeerID,
  //     type: 'offer',
  //     desc:'sessionDescription'
  //   });

  // }

});

socket.on('log', function(array) {
  console.log.apply(console, array);
});

////////////////////////////////////////////////

function sendMessage(message) {
  console.log('Client sending message: ', message);
  log('Client sending message: ', message);
  socket.emit('message', message);
}

// // This client receives a message
// socket.on('message', function(message) {
//   console.log('Client received message:', message);
//   log('Client received message:', message);


//   if (!message.offer && remotePeerID && remotePeerID != message.from) {
//       console.log('Dropping message from unknown peer', message);
//       log('Dropping message from unknown peer', message);
//       return;
//   }


//   if (message === 'got user media') {
//     maybeStart();
//   } else if (message.type === 'offer') {
//     if (!isInitiator && !isStarted) {
//       maybeStart();
//     }
//     remotePeerID=message.from;
//     log('got offfer from remotePeerID: ' + remotePeerID);

//     pc.setRemoteDescription(new RTCSessionDescription(message.desc));
//     doAnswer();
//   } else if (message.type === 'answer' && isStarted) {
//     pc.setRemoteDescription(new RTCSessionDescription(message.desc));
//   } else if (message.type === 'candidate' && isStarted) {
//     var candidate = new RTCIceCandidate({
//       sdpMLineIndex: message.candidate.sdpMLineIndex,
//       sdpMid: message.candidate.sdpMid,
//       candidate: message.candidate.candidate
//     });
//     pc.addIceCandidate(candidate);
//   } else if (message.type === 'bye' && isStarted) {
//     handleRemoteHangup();
//   }
// });

////////////////////////////////////////////////////

var localVideo = document.querySelector('#localVideo');
var remoteVideo = document.querySelector('#remoteVideo');

// navigator.mediaDevices.getUserMedia({
//   audio: true,
//   video: true
// })
// .then(gotStream)
// .catch(function(e) {
//   alert('getUserMedia() error: ' + e.name);
// });

function gotStream(stream) {
  console.log('Adding local stream.');
  localStream = stream;
  localVideo.srcObject = stream;
  sendMessage('got user media');
    isInitiator = true;
  if (isInitiator) {
    maybeStart();
  }
}


// if (location.hostname !== 'localhost') {
//   requestTurn(
//     'https://computeengineondemand.appspot.com/turn?username=41784574&key=4080218913'
//   );
// }

function maybeStart() {
  console.log('>>>>>>> maybeStart() ', isStarted, localStream, isChannelReady);
  if (!isStarted && typeof localStream !== 'undefined' && isChannelReady) {
    console.log('>>>>>> creating peer connection');
    createPeerConnection();
    pc.addStream(localStream);
    isStarted = true;
    console.log('isInitiator', isInitiator);
   // if (isInitiator) {
     // doCall();
   // }
      if (room !== '') {
        socket.emit('create or join', room);
        console.log('Attempted to create or  join room', room);
      }


  }
}

window.onbeforeunload = function() {
    sendMessage({
      from: peerID,
      to: remotePeerID,
      type: 'bye'
    });
};

/////////////////////////////////////////////////////////

function createPeerConnection() {
  try {
    pc = new RTCPeerConnection(null);
    pc.onicecandidate = handleIceCandidate;
    pc.onaddstream = handleRemoteStreamAdded;
    pc.onremovestream = handleRemoteStreamRemoved;
    console.log('Created RTCPeerConnnection');
  } catch (e) {
    console.log('Failed to create PeerConnection, exception: ' + e.message);
    alert('Cannot create RTCPeerConnection object.');
    return;
  }
}

function handleIceCandidate(event) {
  console.log('icecandidate event: ', event);
  if (event.candidate) {
    sendMessage({
      from: peerID,
      to: remotePeerID,
      type: 'candidate',
      candidate: event.candidate
    });
  } else {
    console.log('End of candidates.');
  }
}

function handleCreateOfferError(event) {
  console.log('createOffer() error: ', event);
}

function doCall() {
  console.log('Sending offer to peer');
  pc.createOffer(setLocalAndSendMessage, handleCreateOfferError);
}

function doAnswer() {
  console.log('Sending answer to peer.');
  pc.createAnswer().then(
    setLocalAndSendMessage,
    onCreateSessionDescriptionError
  );
}

function setLocalAndSendMessage(sessionDescription) {
  pc.setLocalDescription(sessionDescription);
  console.log('setLocalAndSendMessage sending message', sessionDescription);

   sendMessage ({
      from: peerID,
      to: remotePeerID,
      type: sessionDescription.type,
      desc:sessionDescription
    });
}

function onCreateSessionDescriptionError(error) {
  log('Failed to create session description: ' + error.toString());
  console.log('Failed to create session description: ' + error.toString());
  
}

function requestTurn(turnURL) {
  var turnExists = false;
  for (var i in pcConfig.iceServers) {
    if (pcConfig.iceServers[i].urls.substr(0, 5) === 'turn:') {
      turnExists = true;
      turnReady = true;
      break;
    }
  }
  if (!turnExists) {
    console.log('Getting TURN server from ', turnURL);
    // No TURN server. Get one from computeengineondemand.appspot.com:
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
      if (xhr.readyState === 4 && xhr.status === 200) {
        var turnServer = JSON.parse(xhr.responseText);
        console.log('Got TURN server: ', turnServer);
        pcConfig.iceServers.push({
          'urls': 'turn:' + turnServer.username + '@' + turnServer.turn,
          'credential': turnServer.password
        });
        turnReady = true;
      }
    };
    xhr.open('GET', turnURL, true);
    xhr.send();
  }
}

function handleRemoteStreamAdded(event) {
  console.log('Remote stream added.');
  remoteStream = event.stream;
  remoteVideo.srcObject = remoteStream;
}

function handleRemoteStreamRemoved(event) {
  console.log('Remote stream removed. Event: ', event);
}

function hangup() {
  console.log('Hanging up.');
  stop();
  sendMessage({
      from: peerID,
      to: remotePeerID,
      type: 'bye'
    });
}

function handleRemoteHangup() {
  console.log('Session terminated.');
  stop();
  //isInitiator = false;
}

function stop() {
  isStarted = false;
  pc.close();
  pc = null;
  localStream=null;
}





$(document).ready(function(){
 

  $("#btn_connect").click(function(){

    console.log("arvind");
          $("#div1").html("<h2>btn_connect!</h2>");
   socket.emit('create or join', room);
 // const data = await socket.request('getRouterRtpCapabilities');

  });


  $("#btn_webcam").click(function(){

    console.log("btn_webcam click");
          $("#div1").html("<h2>btn_webcam</h2>");

   publish();
  });


  $("#btn_subscribe").click(function(){

    console.log("btn_subscribe click");
          $("#div1").html("<h2>btn_subscribe</h2>");

   subscribe();
  });


 });
