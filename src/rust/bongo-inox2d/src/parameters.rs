use crate::abi::Range;
use glam::Vec2;
use serde_json::Value;
use std::{collections::HashMap, path::Path};

pub struct Parameter {
    pub minimum: Vec2,
    pub maximum: Vec2,
    pub value: Vec2,
}
pub struct Alias {
    pub name: String,
    pub axis: usize,
    pub minimum: f32,
    pub maximum: f32,
}
pub struct Parameters {
    pub values: HashMap<String, Parameter>,
    aliases: HashMap<String, Alias>,
}

impl Parameters {
    pub fn new(json: &Value, directory: &Path) -> Result<Self, String> {
        let mut values = HashMap::new();
        for param in json["param"].as_array().ok_or("Missing INP parameters")? {
            let pair = |key: &str| {
                Vec2::new(
                    param[key][0].as_f64().unwrap() as f32,
                    param[key][1].as_f64().unwrap() as f32,
                )
            };
            values.insert(
                param["name"].as_str().unwrap().to_owned(),
                Parameter {
                    minimum: pair("min"),
                    maximum: pair("max"),
                    value: pair("defaults"),
                },
            );
        }
        let path = directory.join("bongocat.bindings.json");
        let mut aliases = HashMap::new();
        if path.exists() {
            use std::io::Read;
            let mut data = Vec::new();
            std::fs::File::open(&path)
                .map_err(|e| e.to_string())?
                .take(65537)
                .read_to_end(&mut data)
                .map_err(|e| e.to_string())?;
            if data.len() > 65536 {
                return Err("Inox2D bindings exceed 64 KiB".into());
            }
            let json: Value = serde_json::from_slice(&data).map_err(|e| e.to_string())?;
            for (id, value) in json.as_object().ok_or("Bindings must be an object")? {
                let name = value["parameter"]
                    .as_str()
                    .ok_or("Missing alias parameter")?
                    .to_owned();
                let axis = match value["axis"].as_str().unwrap_or("x") {
                    "x" => 0,
                    "y" => 1,
                    _ => return Err("Alias axis must be x or y".into()),
                };
                let target = values
                    .get(&name)
                    .ok_or("Alias refers to an unknown parameter")?;
                let minimum = value["min"]
                    .as_f64()
                    .map(|v| v as f32)
                    .unwrap_or(target.minimum[axis]);
                let maximum = value["max"]
                    .as_f64()
                    .map(|v| v as f32)
                    .unwrap_or(target.maximum[axis]);
                if !minimum.is_finite() || !maximum.is_finite() || maximum <= minimum {
                    return Err("Alias range must be finite and increasing".into());
                }
                aliases.insert(
                    id.clone(),
                    Alias {
                        name,
                        axis,
                        minimum,
                        maximum,
                    },
                );
            }
        }
        Ok(Self { values, aliases })
    }
    fn resolve(&self, id: &str) -> Option<Alias> {
        if let Some(a) = self.aliases.get(id) {
            return Some(Alias {
                name: a.name.clone(),
                axis: a.axis,
                minimum: a.minimum,
                maximum: a.maximum,
            });
        }
        let (name, axis) = if self.values.contains_key(id) {
            (id, 0)
        } else if let Some(name) = id.strip_suffix(".x") {
            (name, 0)
        } else if let Some(name) = id.strip_suffix(".y") {
            (name, 1)
        } else {
            return None;
        };
        let parameter = self.values.get(name)?;
        Some(Alias {
            name: name.to_owned(),
            axis,
            minimum: parameter.minimum[axis],
            maximum: parameter.maximum[axis],
        })
    }
    pub fn set(&mut self, id: &str, value: f32) -> bool {
        if !value.is_finite() {
            return false;
        }
        let Some(alias) = self.resolve(id) else {
            return false;
        };
        let param = self.values.get_mut(&alias.name).unwrap();
        let fraction = if alias.maximum > alias.minimum {
            ((value - alias.minimum) / (alias.maximum - alias.minimum)).clamp(0.0, 1.0)
        } else {
            0.0
        };
        param.value[alias.axis] = param.minimum[alias.axis]
            + fraction * (param.maximum[alias.axis] - param.minimum[alias.axis]);
        true
    }
    pub fn range(&self, id: &str) -> Option<Range> {
        let alias = self.resolve(id)?;
        let param = self.values.get(&alias.name)?;
        let span = param.maximum[alias.axis] - param.minimum[alias.axis];
        let fraction = if span > 0.0 {
            (param.value[alias.axis] - param.minimum[alias.axis]) / span
        } else {
            0.0
        };
        Some(Range {
            minimum: alias.minimum,
            maximum: alias.maximum,
            value: alias.minimum + fraction * (alias.maximum - alias.minimum),
        })
    }
}
