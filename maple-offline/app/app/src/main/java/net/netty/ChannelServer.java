package net.netty;

import android.content.Context;
import io.netty.bootstrap.ServerBootstrap;
import io.netty.channel.Channel;
import io.netty.channel.EventLoopGroup;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.nio.NioServerSocketChannel;

public class ChannelServer extends AbstractServer {
    private final int world;
    private final int channel;
    private Channel nettyChannel;
    private Context context;

    public ChannelServer(int port, int world, int channel, Context context) {
        super(port);
        this.world = world;
        this.channel = channel;
        this.context = context;
    }

    @Override
    public void start() {
        EventLoopGroup parentGroup = new NioEventLoopGroup();
        EventLoopGroup childGroup = new NioEventLoopGroup();
        ServerBootstrap bootstrap = new ServerBootstrap()
                .group(parentGroup, childGroup)
                .channel(NioServerSocketChannel.class)
                .childHandler(new ChannelServerInitializer(world, channel, context));

        // maple-offline: the game is on this phone only; nothing else on the
        // network can reach the server (logins take any password)
        this.nettyChannel = bootstrap.bind("127.0.0.1", port).syncUninterruptibly().channel();
    }

    @Override
    public void stop() {
        if (nettyChannel == null) {
            throw new IllegalStateException("Must start ChannelServer before stopping it");
        }

        nettyChannel.close().syncUninterruptibly();
    }
}
