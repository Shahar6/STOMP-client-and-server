package bgu.spl.net.impl.stomp;

import java.util.function.Supplier;

import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.srv.BaseServer;
import bgu.spl.net.srv.Server;

public class StompServer {

    public static void main(String[] args) {
        // TODO: implement this
        int port;
        Supplier<StompMessagingProtocol<String>> protocolFactory ;
        Supplier<StompMessageEncoderDecoder<String>> encdecFactory;   
                  // you can use any server... 
        Server.reactor( 3, 
            7777, //port
            () -> new StompMessagingProtocolImpl<>(), //protocol factory
            StompMessageEncoderDecoder::new //message encoder decoder factory
    ).serve();

    // Server.reactor(
    //         Runtime.getRuntime().availableProcessors(),
    //         7777, //port
    //         () ->  new RemoteCommandInvocationProtocol<>(feed), //protocol factory
    //         ObjectEncoderDecoder::new //message encoder decoder factory
    // ).serve();
        
    }
}
