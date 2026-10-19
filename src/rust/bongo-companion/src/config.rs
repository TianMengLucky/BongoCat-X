use bongo_plugin::io::{json, Value};
use bongo_plugin::text::buffer;
use std::path::Path;

#[derive(Clone)]
pub struct Config {
    pub enabled: u32,
    pub endpoint: [u8; 2048],
    pub model: [u8; 128],
    pub key_env: [u8; 128],
    pub prompt: [u8; 2048],
    pub triggers: [u32; 5],
    pub cooldown: f64,
    pub idle: f64,
    pub duration: f64,
}
impl Default for Config {
    fn default() -> Self {
        Self {
            enabled: 0,
            endpoint: buffer("https://api.deepseek.com/chat/completions"),
            model: buffer("deepseek-flash"),
            key_env: buffer("DEEPSEEK_API_KEY"),
            prompt: buffer("You are a friendly desktop pet. Respond to the event with one short, warm sentence in the user's language. Do not mention technical details."),
            triggers: [1, 1, 0, 0, 1],
            cooldown: 60.0, idle: 300.0, duration: 15.0,
        }
    }
}
fn string<const N: usize>(v: &Value, name: &str) -> Option<[u8; N]> {
    let s = v.get(name)?.as_str()?;
    (!s.is_empty() && s.len() < N && !s.contains('\0')).then(|| buffer(s))
}
impl Config {
    pub fn decode(v: &Value) -> Option<Self> {
        if v.get("version")?.as_u64()? != 1 {
            return None;
        }
        let mut triggers = [0; 5];
        let values = v.get("triggers")?.as_array()?;
        if values.len() != triggers.len() {
            return None;
        }
        for (out, value) in triggers.iter_mut().zip(values) {
            *out = u32::from(value.as_bool()?);
        }
        let result = Self {
            enabled: u32::from(v.get("enabled")?.as_bool()?),
            endpoint: string(v, "endpoint")?,
            model: string(v, "model")?,
            key_env: string(v, "key_env")?,
            prompt: string(v, "prompt")?,
            triggers,
            cooldown: v.get("cooldown")?.as_f64()?,
            idle: v.get("idle")?.as_f64()?,
            duration: v.get("duration")?.as_f64()?,
        };
        result.valid().then_some(result)
    }
    pub fn valid(&self) -> bool {
        use bongo_plugin::page::text;
        self.enabled <= 1
            && self.triggers.iter().all(|&v| v <= 1)
            && (10.0..=3600.0).contains(&self.cooldown)
            && (30.0..=86400.0).contains(&self.idle)
            && (3.0..=120.0).contains(&self.duration)
            && text(&self.endpoint).starts_with("https://")
            && !text(&self.model).trim().is_empty()
            && !text(&self.prompt).trim().is_empty()
            && !text(&self.key_env).is_empty()
            && text(&self.key_env)
                .bytes()
                .all(|b| b.is_ascii_alphanumeric() || b == b'_')
    }
    pub fn encode(&self) -> Value {
        use bongo_plugin::page::text;
        json!({"version":1,"enabled":self.enabled != 0,"endpoint":text(&self.endpoint),
            "model":text(&self.model),"key_env":text(&self.key_env),"prompt":text(&self.prompt),
            "triggers":self.triggers.map(|v| v != 0),"cooldown":self.cooldown,
            "idle":self.idle,"duration":self.duration})
    }
    pub fn load(file: &Path) -> Result<Self, ()> {
        match bongo_plugin::io::load(file)? {
            Some(v) => Self::decode(&v).ok_or(()),
            None => Ok(Self::default()),
        }
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn config_round_trip_and_bounds() {
        let c = Config::default();
        assert!(c.valid());
        assert_eq!(Config::decode(&c.encode()).unwrap().enabled, 0);
        let mut v = c.encode();
        v["cooldown"] = json!(0);
        assert!(Config::decode(&v).is_none());
        v = c.encode();
        v["endpoint"] = json!("http://example.com");
        assert!(Config::decode(&v).is_none());
        v = c.encode();
        v["triggers"] = json!([true]);
        assert!(Config::decode(&v).is_none());
        assert!(!c.encode().to_string().contains("api_key"));
    }
}
