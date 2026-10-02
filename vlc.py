import vlc


instance = vlc.Instance(
    "--extraintf=http",
    "--http-host=127.0.0.1",
    "--http-port=8080"
)

player = instance.media_player_new()


def get_now_playing():
    media = player.get_media()

    if media is None:
        return {
            "playing": False,
            "title": None,
            "artist": None
        }

    return {
        "playing": player.is_playing(),
        "title": media.get_meta(vlc.Meta.Title),
        "artist": media.get_meta(vlc.Meta.Artist),
        "album": media.get_meta(vlc.Meta.Album)
    }