package bgu.spl.net.srv;

import java.io.IOException;

public interface Connections<T> {


    boolean send(int connectionId, T msg, T receipt);

    void send(String channel, T msg);

    void disconnect(int connectionId);

}
